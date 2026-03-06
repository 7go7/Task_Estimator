// backend/main.cpp

#include "crow_all.h"
#include <pqxx/pqxx>
#include <xgboost/c_api.h>

#include <iostream>
#include <curl/curl.h>
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>
#include <cstdint>
#include <cctype>

// ==============================
// Hashing Trick (Python-identical)
// ==============================

const int NUM_BINS = 8192;

// Stable FNV-1a 32-bit hash
uint32_t fnv1a_32(const std::string& text) {
    uint32_t hash = 0x811c9dc5;
    for (char c : text) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 0x01000193;
    }
    return hash;
}

// Tokenization: letters/digits only, lowercase
std::vector<std::string> extract_words(const std::string& text) {
    std::vector<std::string> words;
    std::string current_word;

    for (char c : text) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc)) {
            current_word += static_cast<char>(std::tolower(uc));
        } else if (!current_word.empty()) {
            words.push_back(current_word);
            current_word.clear();
        }
    }
    if (!current_word.empty()) words.push_back(current_word);
    return words;
}

// Hashing Trick vectorization (binary presence)
std::vector<float> vectorize_text_hashed(const std::string& text) {
    std::vector<float> vector(NUM_BINS, 0.0f);
    std::vector<std::string> words = extract_words(text);

    // Unigrams
    for (const auto& w : words) {
        uint32_t idx = fnv1a_32(w) % NUM_BINS;
        vector[idx] = 1.0f;
    }

    // Bigrams
    for (size_t i = 0; i + 1 < words.size(); ++i) {
        std::string bigram = words[i] + " " + words[i + 1];
        uint32_t idx = fnv1a_32(bigram) % NUM_BINS;
        vector[idx] = 1.0f;
    }

    return vector;
}

// ==============================
// Prediction
// ==============================

float predict_hours(BoosterHandle booster, int priority, int role, const std::vector<float>& text_features) {
    std::vector<float> features;
    features.reserve(2 + text_features.size());

    features.push_back(static_cast<float>(priority));
    features.push_back(static_cast<float>(role));
    features.insert(features.end(), text_features.begin(), text_features.end());

    DMatrixHandle dmatrix;
    XGDMatrixCreateFromMat(features.data(), 1, static_cast<bst_ulong>(features.size()), -1, &dmatrix);

    bst_ulong out_len = 0;
    const float* out_result = nullptr;

    XGBoosterPredict(booster, dmatrix, 0, 0, 0, &out_len, &out_result);

    float result = (out_result && out_len > 0) ? out_result[0] : 0.1f;
    XGDMatrixFree(dmatrix);

    if (result < 0.1f) result = 0.1f;
    return result;
}

// Функція відправки коментаря в Jira
void send_jira_comment(const std::string& issue_key, float hours) {
    std::string domain = "sono109"; 
    std::string email = "dallkil012@gmail.com";
    std::string api_token = ""; 

    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    
    if(curl) {
        std::string url = "https://" + domain + ".atlassian.net/rest/api/2/issue/" + issue_key + "/comment";
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // Налаштування заголовків
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Accept: application/json");
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Налаштування авторизації (Basic Auth)
        std::string auth = email + ":" + api_token;
        curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());

        std::stringstream stream;
        stream << std::fixed << std::setprecision(1) << hours;
        std::string hours_str = stream.str();

        std::string json_body = "{\"body\": \" *AI Task Estimator* \\nМодель машинного навчання (XGBoost) оцінила цю задачу в *" + hours_str + " годин* на основі історичних даних команди.\"}";
        
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());

        // Виконуємо запит (HTTPS)
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
}

int main() {
    crow::SimpleApp app;

    // 1) Load XGBoost model
    BoosterHandle booster;
    XGBoosterCreate(NULL, 0, &booster);
    if (XGBoosterLoadModel(booster, "model/task_estimation_model.json") != 0) {
        std::cerr << "Error loading model!" << std::endl;
        return 1;
    }
    std::cout << "ML Model loaded successfully!" << std::endl;

    // 2) DB connection string
    std::string db_conn = "postgresql://admin:secretpassword@localhost:5432/task_estimator";

    // API: Create task
    CROW_ROUTE(app, "/api/task").methods(crow::HTTPMethod::POST)
    ([&db_conn, booster](const crow::request& req) {
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400);

        std::string title = x["title"].s();
        std::string desc  = x["description"].s();
        int priority      = x["priority_code"].i();
        int role          = x["role_code"].i();

        // Hashing Trick features (8192)
        std::vector<float> text_features = vectorize_text_hashed(desc);

        // Predict
        float predicted_hours = predict_hours(booster, priority, role, text_features);

        try {
            pqxx::connection c(db_conn);
            pqxx::work w(c);
            w.exec_params(
                "INSERT INTO Tasks (title, description, priority_code, role_code, ml_prediction) "
                "VALUES ($1, $2, $3, $4, $5)",
                title, desc, priority, role, predicted_hours
            );
            w.commit();

            crow::json::wvalue res;
            res["status"] = "created";
            res["ml_prediction"] = predicted_hours;
            return crow::response(res);
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    });

    // API: Get all tasks
    CROW_ROUTE(app, "/api/tasks").methods(crow::HTTPMethod::GET)
    ([&db_conn]() {
        try {
            pqxx::connection c(db_conn);
            pqxx::work w(c);
            pqxx::result r = w.exec(
                "SELECT id, title, priority_code, role_code, ml_prediction, actual_hours "
                "FROM Tasks ORDER BY id DESC"
            );

            std::vector<crow::json::wvalue> tasks;
            tasks.reserve(r.size());

            for (auto row : r) {
                crow::json::wvalue task;
                task["id"] = row[0].as<int>();
                task["title"] = row[1].as<std::string>();
                task["priority_code"] = row[2].is_null() ? 1 : row[2].as<int>();
                task["role_code"] = row[3].is_null() ? 1 : row[3].as<int>();
                task["ml_prediction"] = row[4].is_null() ? 0.0 : row[4].as<float>();
                task["actual_hours"] = row[5].is_null() ? -1.0 : row[5].as<float>();
                tasks.push_back(std::move(task));
            }

            crow::json::wvalue response;
            response["tasks"] = std::move(tasks);
            return crow::response(200, response);
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    });

    // API: Resolve task (training trigger)
    CROW_ROUTE(app, "/api/task/<int>/resolve").methods(crow::HTTPMethod::POST)
    ([&db_conn](const crow::request& req, int task_id) {
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400);

        float actual = static_cast<float>(x["actual_hours"].d());

        try {
            pqxx::connection c(db_conn);
            pqxx::work w(c);
            w.exec_params("UPDATE Tasks SET actual_hours = $1 WHERE id = $2", actual, task_id);
            w.commit();

            crow::json::wvalue res;
            res["status"] = "resolved";
            return crow::response(res);
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    });

    // ==============================
    // API: Webhook для Jira
    // ==============================
    CROW_ROUTE(app, "/api/webhook/jira").methods(crow::HTTPMethod::POST)
    ([&db_conn, booster](const crow::request& req) {
        auto j = crow::json::load(req.body);
        if (!j) return crow::response(400, "Invalid JSON from Jira");

        try {
            // 0. Дізнаємось тип події (Створення чи Оновлення)
            std::string webhook_event = "";
            if (j.has("webhookEvent") && j["webhookEvent"].t() == crow::json::type::String) {
                webhook_event = j["webhookEvent"].s();
            }

            // 1. Витягуємо дані з формату Jira
            std::string jira_key = j["issue"]["key"].s();
            
            std::string title = "Untitled";
            if (j["issue"]["fields"].has("summary") && j["issue"]["fields"]["summary"].t() == crow::json::type::String) {
                title = j["issue"]["fields"]["summary"].s();
            }
            
            std::string desc = "";
            if (j["issue"]["fields"].has("description") && j["issue"]["fields"]["description"].t() == crow::json::type::String) {
                desc = j["issue"]["fields"]["description"].s();
            }

            // 2. Мапінг пріоритетів Jira на наші цифри (0-3)
            std::string jira_priority = "Medium";
            if (j["issue"]["fields"].has("priority") && j["issue"]["fields"]["priority"].t() != crow::json::type::Null) {
                if (j["issue"]["fields"]["priority"].has("name") && j["issue"]["fields"]["priority"]["name"].t() == crow::json::type::String) {
                    jira_priority = j["issue"]["fields"]["priority"]["name"].s();
                }
            }
            
            int priority = 1; // За замовчуванням Medium
            if (jira_priority == "Highest" || jira_priority == "Critical") priority = 3;
            else if (jira_priority == "High") priority = 2;
            else if (jira_priority == "Low" || jira_priority == "Lowest") priority = 0;

            int role = 1; // Ставимо Middle (1) для зовнішніх задач

            // 3. Векторизація Hashing Trick та Прогноз
            std::vector<float> text_features = vectorize_text_hashed(desc);
            float predicted_hours = predict_hours(booster, priority, role, text_features);

            // 4. Отримуємо ФАКТИЧНИЙ ЧАС (Time Spent) з Jira
            // Jira зберігає залогований час у секундах
            float actual_hours = -1.0f;
            if (j["issue"]["fields"].has("timespent") && j["issue"]["fields"]["timespent"].t() == crow::json::type::Number) {
                int timespent_sec = j["issue"]["fields"]["timespent"].i();
                if (timespent_sec > 0) {
                    actual_hours = timespent_sec / 3600.0f; // Переводимо секунди в години
                }
            }

            // 5. Збереження в Базу Даних
            pqxx::connection c(db_conn);
            pqxx::work w(c);
            
            if (actual_hours > 0) {
                // Якщо час залоговано, оновлюємо actual_hours
                w.exec_params(
                    "INSERT INTO Tasks (title, description, priority_code, role_code, ml_prediction, jira_key, actual_hours) "
                    "VALUES ($1, $2, $3, $4, $5, $6, $7) "
                    "ON CONFLICT (jira_key) DO UPDATE "
                    "SET title = EXCLUDED.title, description = EXCLUDED.description, actual_hours = EXCLUDED.actual_hours",
                    title, desc, priority, role, predicted_hours, jira_key, actual_hours
                );
            } else {
                // Якщо часу ще немає, зберігаємо тільки задачу
                w.exec_params(
                    "INSERT INTO Tasks (title, description, priority_code, role_code, ml_prediction, jira_key) "
                    "VALUES ($1, $2, $3, $4, $5, $6) "
                    "ON CONFLICT (jira_key) DO UPDATE "
                    "SET title = EXCLUDED.title, description = EXCLUDED.description",
                    title, desc, priority, role, predicted_hours, jira_key
                );
            }
            w.commit();

            // 6. Відправляємо ML-прогноз назад у Jira (ТІЛЬКИ при створенні!)
            if (webhook_event == "jira:issue_created") {
                send_jira_comment(jira_key, predicted_hours);
                std::cout << "[Jira Webhook] NEW TASK " << jira_key << " | Prediction: " << predicted_hours << "h | Comment sent.\n";
            } else {
                std::cout << "[Jira Webhook] UPDATED TASK " << jira_key << " | Actual Hours logged: " << (actual_hours > 0 ? std::to_string(actual_hours) : "None") << "\n";
            }

            return crow::response(200, "Webhook processed successfully");

        } catch (const std::exception& e) {
            std::cerr << "[Jira Webhook Error] " << e.what() << '\n';
            return crow::response(500, e.what());
        }
    });

    app.port(8080).multithreaded().run();
}