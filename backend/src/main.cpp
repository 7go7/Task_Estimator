#include "crow_all.h"
#include <pqxx/pqxx>
#include <xgboost/c_api.h>
#include <curl/curl.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <cctype>
#include <stdexcept>

using namespace std;

static const int NUM_BINS = 8192;

struct UserContext
{
    int user_id;
    int organization_id;
    string role;
    string email;
};

static void log_line(const std::string &msg)
{
    std::cerr << "[jira-sync] " << msg << std::endl;
}

string env_or(const string &key, const string &fallback = "")
{
    const char *v = std::getenv(key.c_str());
    return v ? string(v) : fallback;
}

string db_conn_string()
{
    return "host=" + env_or("POSTGRES_HOST", "localhost") +
           " port=" + env_or("POSTGRES_PORT", "5432") +
           " dbname=" + env_or("POSTGRES_DB", "task_estimator") +
           " user=" + env_or("POSTGRES_USER", "admin") +
           " password=" + env_or("POSTGRES_PASSWORD", "secretpassword");
}

string iso_now_plus_days(int days)
{
    auto now = std::chrono::system_clock::now() + std::chrono::hours(24 * days);
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

string sha256(const string &input)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(input.c_str()), input.size(), hash);
    std::ostringstream oss;
    for (unsigned char c : hash)
    {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    return oss.str();
}

string random_token(size_t bytes = 32)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<int> dist(0, 255);
    vector<unsigned char> raw(bytes);
    for (size_t i = 0; i < bytes; ++i)
        raw[i] = static_cast<unsigned char>(dist(gen));
    string hex;
    static const char *alphabet = "0123456789abcdef";
    for (unsigned char c : raw)
    {
        hex.push_back(alphabet[c >> 4]);
        hex.push_back(alphabet[c & 0x0F]);
    }
    return hex;
}

string slugify(const string &input)
{
    string out;
    for (char c : input)
    {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc))
            out.push_back(static_cast<char>(std::tolower(uc)));
        else if (c == ' ' || c == '_' || c == '-')
            out.push_back('-');
    }
    while (!out.empty() && out.back() == '-')
        out.pop_back();
    if (out.empty())
        out = "org-" + random_token(4);
    return out;
}

uint32_t fnv1a_32(const std::string &text)
{
    uint32_t hash = 0x811c9dc5;
    for (char c : text)
    {
        hash ^= static_cast<uint8_t>(c);
        hash *= 0x01000193;
    }
    return hash;
}

vector<string> extract_words(const string &text)
{
    vector<string> words;
    string current;
    for (char c : text)
    {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc))
            current += static_cast<char>(std::tolower(uc));
        else if (!current.empty())
        {
            words.push_back(current);
            current.clear();
        }
    }
    if (!current.empty())
        words.push_back(current);
    return words;
}

vector<float> vectorize_text_hashed(const string &text)
{
    vector<float> v(NUM_BINS, 0.0f);
    auto words = extract_words(text);
    for (const auto &w : words)
        v[fnv1a_32(w) % NUM_BINS] = 1.0f;
    for (size_t i = 0; i + 1 < words.size(); ++i)
        v[fnv1a_32(words[i] + " " + words[i + 1]) % NUM_BINS] = 1.0f;
    return v;
}

int map_priority_code(const string &priority_name)
{
    string p = priority_name;
    for (char &c : p)
        c = static_cast<char>(tolower(c));
    if (p.find("highest") != string::npos || p.find("critical") != string::npos || p.find("blocker") != string::npos)
        return 3;
    if (p.find("high") != string::npos)
        return 2;
    if (p.find("low") != string::npos || p.find("lowest") != string::npos)
        return 0;
    return 1;
}

struct PredictionResult
{
    float hours;
    float confidence;
    string model_path;
};

PredictionResult predict_hours_for_org(int organization_id, int priority, int role, const string &text)
{
    string model_dir = env_or("MODEL_DIR", "/app/model");
    string org_model = model_dir + "/org_" + to_string(organization_id) + "_task_estimation_model.json";
    string foundation_model = model_dir + "/foundation_task_estimation_model.json";
    string model_path = std::filesystem::exists(org_model) ? org_model : foundation_model;

    BoosterHandle booster;
    XGBoosterCreate(nullptr, 0, &booster);
    if (XGBoosterLoadModel(booster, model_path.c_str()) != 0)
    {
        XGBoosterFree(booster);
        return {0.1f, 0.25f, "unavailable"};
    }

    vector<float> text_features = vectorize_text_hashed(text);
    vector<float> features;
    features.reserve(2 + text_features.size());
    features.push_back(static_cast<float>(priority));
    features.push_back(static_cast<float>(role));
    features.insert(features.end(), text_features.begin(), text_features.end());

    DMatrixHandle dmat;
    XGDMatrixCreateFromMat(features.data(), 1, static_cast<bst_ulong>(features.size()), -1, &dmat);
    bst_ulong out_len = 0;
    const float *out_result = nullptr;
    XGBoosterPredict(booster, dmat, 0, 0, 0, &out_len, &out_result);
    float pred = (out_result && out_len > 0) ? out_result[0] : 0.1f;
    if (pred < 0.1f)
        pred = 0.1f;
    XGDMatrixFree(dmat);
    XGBoosterFree(booster);

    float confidence = std::filesystem::exists(org_model) ? 0.82f : 0.58f;
    return {pred, confidence, model_path};
}

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

string base64_encode(const string &input)
{
    int encoded_len = 4 * ((input.size() + 2) / 3);
    vector<unsigned char> out(encoded_len + 1);
    EVP_EncodeBlock(out.data(), reinterpret_cast<const unsigned char *>(input.data()), static_cast<int>(input.size()));
    return string(reinterpret_cast<char *>(out.data()));
}

struct HttpResult
{
    long status;
    string body;
};

HttpResult http_request(const string &method, const string &url, const vector<string> &headers, const string &body = "")
{
    CURL *curl = curl_easy_init();
    string response;
    long status = 0;
    if (!curl)
        return {500, "curl init failed"};

    struct curl_slist *chunk = nullptr;
    for (const auto &h : headers)
        chunk = curl_slist_append(chunk, h.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    if (method == "POST")
    {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    }
    if (method == "PUT")
    {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    }

    auto res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        response = curl_easy_strerror(res);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);
    return {status, response};
}

string json_escape(const string &s)
{
    string out;
    out.reserve(s.size() + 16);
    for (char ch : s)
    {
        switch (ch)
        {
        case '\"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += ch;
            break;
        }
    }
    return out;
}

string flatten_adf(const crow::json::rvalue &node)
{
    if (node.t() == crow::json::type::String)
        return string(node.s());
    string out;
    if (node.t() == crow::json::type::Object)
    {
        if (node.has("text") && node["text"].t() == crow::json::type::String)
            out += string(node["text"].s()) + " ";
        if (node.has("content") && node["content"].t() == crow::json::type::List)
        {
            for (const auto &child : node["content"])
                out += flatten_adf(child);
        }
    }
    if (node.t() == crow::json::type::List)
    {
        for (const auto &child : node)
            out += flatten_adf(child);
    }
    return out;
}

optional<UserContext> require_auth(const crow::request &req, pqxx::connection &c)
{
    auto auth = req.get_header_value("Authorization");
    if (auth.rfind("Bearer ", 0) != 0)
        return std::nullopt;
    string token = auth.substr(7);
    pqxx::work w(c);
    auto r = w.exec_params(
        "SELECT u.id, u.organization_id, u.role, u.email "
        "FROM sessions s JOIN users u ON s.user_id = u.id "
        "WHERE s.token = $1 AND s.expires_at > NOW()",
        token);
    if (r.empty())
        return std::nullopt;
    return UserContext{r[0][0].as<int>(), r[0][1].as<int>(), r[0][2].c_str(), r[0][3].c_str()};
}

void log_usage(pqxx::work &w, int organization_id, optional<int> user_id, optional<int> jira_issue_row_id, const string &action, int tokens)
{
    if (user_id.has_value() && jira_issue_row_id.has_value())
    {
        w.exec_params("INSERT INTO token_usage (organization_id, user_id, jira_issue_id, action_type, tokens_used) VALUES ($1,$2,$3,$4,$5)",
                      organization_id, user_id.value(), jira_issue_row_id.value(), action, tokens);
    }
    else if (user_id.has_value())
    {
        w.exec_params("INSERT INTO token_usage (organization_id, user_id, action_type, tokens_used) VALUES ($1,$2,$3,$4)",
                      organization_id, user_id.value(), action, tokens);
    }
    else
    {
        w.exec_params("INSERT INTO token_usage (organization_id, action_type, tokens_used) VALUES ($1,$2,$3)",
                      organization_id, action, tokens);
    }
}

void sync_single_issue(pqxx::work &w, int organization_id, const crow::json::rvalue &issue)
{
    auto fields = issue["fields"];

    string key = (issue.has("key") && issue["key"].t() == crow::json::type::String)
                     ? string(issue["key"].s())
                     : "";

    string issue_id = (issue.has("id") && issue["id"].t() == crow::json::type::String)
                          ? string(issue["id"].s())
                          : "";

    string summary = (fields.has("summary") && fields["summary"].t() == crow::json::type::String)
                         ? string(fields["summary"].s())
                         : "Untitled";

    string description = "";
    if (fields.has("description") && fields["description"].t() != crow::json::type::Null)
    {
        description = flatten_adf(fields["description"]);
    }

    string status = "Unknown";
    if (fields.has("status") &&
        fields["status"].t() == crow::json::type::Object &&
        fields["status"].has("name") &&
        fields["status"]["name"].t() == crow::json::type::String)
    {
        status = string(fields["status"]["name"].s());
    }

    string priority = "Medium";
    if (fields.has("priority") &&
        fields["priority"].t() == crow::json::type::Object &&
        fields["priority"].has("name") &&
        fields["priority"]["name"].t() == crow::json::type::String)
    {
        priority = string(fields["priority"]["name"].s());
    }

    string issue_type = "Task";
    if (fields.has("issuetype") &&
        fields["issuetype"].t() == crow::json::type::Object &&
        fields["issuetype"].has("name") &&
        fields["issuetype"]["name"].t() == crow::json::type::String)
    {
        issue_type = string(fields["issuetype"]["name"].s());
    }

    string assignee = "Unassigned";
    if (fields.has("assignee") &&
        fields["assignee"].t() == crow::json::type::Object &&
        fields["assignee"].has("displayName") &&
        fields["assignee"]["displayName"].t() == crow::json::type::String)
    {
        assignee = string(fields["assignee"]["displayName"].s());
    }

    string updated = (fields.has("updated") && fields["updated"].t() == crow::json::type::String)
                         ? string(fields["updated"].s())
                         : "";

    string project_key = "";
    if (fields.has("project") &&
        fields["project"].t() == crow::json::type::Object &&
        fields["project"].has("key") &&
        fields["project"]["key"].t() == crow::json::type::String)
    {
        project_key = string(fields["project"]["key"].s());
    }

    double actual_hours = -1.0;
    if (fields.has("timespent") && fields["timespent"].t() == crow::json::type::Number)
    {
        int timespent_sec = fields["timespent"].i();
        if (timespent_sec > 0)
            actual_hours = timespent_sec / 3600.0;
    }

    int priority_code = map_priority_code(priority);
    int role_code = 1;

    auto prediction = predict_hours_for_org(
        organization_id,
        priority_code,
        role_code,
        summary + " " + description);

    w.exec_params(
        "INSERT INTO jira_issues (organization_id, jira_issue_id, jira_key, jira_project_key, summary, description, issue_type, status, priority_name, priority_code, assignee_name, role_code, estimated_hours, actual_hours, confidence, source_updated_at, local_updated_at) "
        "VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14,$15,NOW(),NOW()) "
        "ON CONFLICT (organization_id, jira_key) DO UPDATE SET "
        "jira_issue_id = EXCLUDED.jira_issue_id, "
        "jira_project_key = EXCLUDED.jira_project_key, "
        "summary = EXCLUDED.summary, "
        "description = EXCLUDED.description, "
        "issue_type = EXCLUDED.issue_type, "
        "status = EXCLUDED.status, "
        "priority_name = EXCLUDED.priority_name, "
        "priority_code = EXCLUDED.priority_code, "
        "assignee_name = EXCLUDED.assignee_name, "
        "role_code = EXCLUDED.role_code, "
        "estimated_hours = EXCLUDED.estimated_hours, "
        "actual_hours = EXCLUDED.actual_hours, "
        "confidence = EXCLUDED.confidence, "
        "source_updated_at = NOW(), "
        "local_updated_at = NOW()",
        organization_id, issue_id, key, project_key, summary, description, issue_type,
        status, priority, priority_code, assignee, role_code,
        prediction.hours, actual_hours, prediction.confidence);
}

bool sync_org_from_jira(pqxx::connection &c, int organization_id, std::string &error_out)
{
    try
    {
        string base_url, user_email, api_token, project_keys_raw;

        {
            pqxx::work w(c);
            auto r = w.exec_params(
                "SELECT base_url, user_email, api_token, project_keys "
                "FROM jira_connections WHERE organization_id = $1",
                organization_id);

            if (r.empty())
            {
                error_out = "No Jira connection saved for this organization";
                log_line(error_out);
                return false;
            }

            base_url = r[0][0].c_str();
            user_email = r[0][1].c_str();
            api_token = r[0][2].c_str();
            project_keys_raw = r[0][3].is_null() ? "" : string(r[0][3].c_str());
            w.commit();
        }

        while (!base_url.empty() && base_url.back() == '/')
            base_url.pop_back();

        log_line("base_url: " + base_url);
        log_line("email: " + user_email);
        log_line("project_keys_raw: " + project_keys_raw);

        if (project_keys_raw.empty())
        {
            error_out = "Jira project keys are empty. Example: KAN";
            log_line(error_out);
            return false;
        }

        vector<string> keys;
        {
            string cur;
            for (char ch : project_keys_raw)
            {
                if (ch == ',')
                {
                    if (!cur.empty())
                        keys.push_back(cur);
                    cur.clear();
                }
                else if (!isspace(static_cast<unsigned char>(ch)))
                {
                    cur.push_back(ch);
                }
            }
            if (!cur.empty())
                keys.push_back(cur);
        }

        if (keys.empty())
        {
            error_out = "No valid Jira project keys found. Example: KAN";
            log_line(error_out);
            return false;
        }

        string quoted;
        for (size_t i = 0; i < keys.size(); ++i)
        {
            if (i)
                quoted += ",";
            quoted += "\"" + keys[i] + "\"";
        }

        string jql = "project in (" + quoted + ") ORDER BY updated DESC";
        log_line("jql: " + jql);

        string payload =
            string("{\"jql\":\"") + json_escape(jql) +
            "\",\"maxResults\":100,"
            "\"fields\":[\"summary\",\"description\",\"status\",\"priority\",\"issuetype\",\"assignee\",\"updated\",\"project\",\"timespent\"]}";

        CURL *escaper = curl_easy_init();
        char *encoded_jql = curl_easy_escape(escaper, jql.c_str(), static_cast<int>(jql.size()));

        string fields =
            "summary,description,status,priority,issuetype,assignee,updated,project,timespent";

        string url = base_url +
                     "/rest/api/3/search/jql?jql=" + string(encoded_jql ? encoded_jql : "") +
                     "&maxResults=100&fields=" + fields;

        if (encoded_jql)
            curl_free(encoded_jql);
        curl_easy_cleanup(escaper);

        CURL *curl = curl_easy_init();
        if (!curl)
        {
            error_out = "curl init failed";
            log_line(error_out);
            return false;
        }

        string response_body;
        long http_code = 0;

        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "Accept: application/json");

        string auth = user_email + ":" + api_token;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        log_line("HTTP code: " + to_string(http_code));
        log_line("Response body: " + response_body);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK)
        {
            error_out = string("curl error: ") + curl_easy_strerror(res);
            log_line(error_out);
            return false;
        }

        if (http_code < 200 || http_code >= 300)
        {
            error_out = string("Jira returned HTTP ") + to_string(http_code) + ": " + response_body;
            log_line(error_out);

            pqxx::work werr(c);
            werr.exec_params(
                "INSERT INTO audit_logs (organization_id, action, details) "
                "VALUES ($1,$2,$3::jsonb)",
                organization_id,
                "jira.sync_failed",
                string("{\"status\":") + to_string(http_code) + "}");
            werr.commit();
            return false;
        }
        auto parsed = crow::json::load(response_body);
        if (!parsed || !parsed.has("issues"))
        {
            error_out = "Jira response JSON is invalid or missing 'issues'";
            log_line(error_out);
            return false;
        }

        int count = 0;
        pqxx::work wsync(c);
        for (const auto &issue : parsed["issues"])
        {
            sync_single_issue(wsync, organization_id, issue);
            ++count;
        }

        log_usage(wsync, organization_id, {}, {}, "jira_sync", 50);
        wsync.exec_params(
            "INSERT INTO audit_logs (organization_id, action, details) "
            "VALUES ($1,$2,$3::jsonb)",
            organization_id,
            "jira.sync_completed",
            string("{\"issues\":") + to_string(count) + "}");
        wsync.commit();
        return true;
    }
    catch (const std::exception &e)
    {
        error_out = string("exception: ") + e.what();
        log_line(error_out);
        return false;
    }
}

bool send_jira_comment_for_org(pqxx::connection &c, int organization_id, const string &issue_key, float hours, string &error_out)
{
    try
    {
        string base_url, user_email, api_token;
        {
            pqxx::work w(c);
            auto r = w.exec_params(
                "SELECT base_url, user_email, api_token FROM jira_connections WHERE organization_id = $1",
                organization_id);
            if (r.empty())
            {
                error_out = "No Jira connection found for organization";
                return false;
            }
            base_url = r[0][0].c_str();
            user_email = r[0][1].c_str();
            api_token = r[0][2].c_str();
            w.commit();
        }

        while (!base_url.empty() && base_url.back() == '/')
            base_url.pop_back();

        std::ostringstream hs;
        hs << std::fixed << std::setprecision(1) << hours;

        string comment_text =
            "AI Task Estimator predicted this issue at " + hs.str() + " hours.";

        string body =
            "{"
            "\"body\":{"
            "\"type\":\"doc\","
            "\"version\":1,"
            "\"content\":[{"
            "\"type\":\"paragraph\","
            "\"content\":[{"
            "\"type\":\"text\","
            "\"text\":\"" +
            json_escape(comment_text) + "\""
                                        "}]"
                                        "}]"
                                        "}"
                                        "}";

        string url = base_url + "/rest/api/3/issue/" + issue_key + "/comment";

        CURL *curl = curl_easy_init();
        if (!curl)
        {
            error_out = "curl init failed";
            return false;
        }

        string response_body;
        long http_code = 0;

        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "Accept: application/json");
        headers = curl_slist_append(headers, "Content-Type: application/json");

        string auth = user_email + ":" + api_token;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK)
        {
            error_out = string("comment curl error: ") + curl_easy_strerror(res);
            return false;
        }
        if (http_code < 200 || http_code >= 300)
        {
            error_out = string("comment HTTP ") + to_string(http_code) + ": " + response_body;
            return false;
        }

        return true;
    }
    catch (const std::exception &e)
    {
        error_out = e.what();
        return false;
    }
}

int main()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    crow::SimpleApp app;
    string db_conn = db_conn_string();
    string cors_origin = env_or("CORS_ORIGIN", "*");
    uint16_t port = static_cast<uint16_t>(stoi(env_or("PORT", "8080")));

    auto add_cors = [&](crow::response &res)
    {
        res.add_header("Access-Control-Allow-Origin", cors_origin);
        res.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
        res.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.add_header("Access-Control-Max-Age", "86400");
    };

    CROW_ROUTE(app, "/api/health").methods(crow::HTTPMethod::GET)([&]()
                                                                  {
        crow::json::wvalue x; x["status"] = "ok"; x["stack"] = "react-cpp-crow-postgres-xgboost-python-jira";
        crow::response res(200, x); add_cors(res); return res; });

    CROW_ROUTE(app, "/api/<path>").methods(crow::HTTPMethod::OPTIONS)([&](const string &)
                                                                      {
        crow::response res(204); add_cors(res); return res; });

    CROW_ROUTE(app, "/api/plans").methods(crow::HTTPMethod::GET)([&]()
                                                                 {
        pqxx::connection c(db_conn);
        pqxx::work w(c);
        auto r = w.exec("SELECT id, code, name, max_users, monthly_predictions, monthly_tokens, private_model_enabled FROM pricing_plans ORDER BY id");
        crow::json::wvalue data;
        vector<crow::json::wvalue> plans;
        for (const auto& row : r) {
            crow::json::wvalue p;
            p["id"] = row[0].as<int>();
            p["code"] = row[1].c_str();
            p["name"] = row[2].c_str();
            p["max_users"] = row[3].as<int>();
            p["monthly_predictions"] = row[4].as<int>();
            p["monthly_tokens"] = row[5].as<int>();
            p["private_model_enabled"] = row[6].as<bool>();
            plans.push_back(std::move(p));
        }
        data["plans"] = std::move(plans);
        crow::response res(200, data); add_cors(res); return res; });

    CROW_ROUTE(app, "/api/auth/register").methods(crow::HTTPMethod::POST)([&](const crow::request &req)
                                                                          {
        auto x = crow::json::load(req.body);
        if (!x) { crow::response res(400, "Invalid JSON"); add_cors(res); return res; }
        try {
            string org_name = x["company_name"].s();
            string slug = slugify(org_name);
            string first_name = x["first_name"].s();
            string last_name = x["last_name"].s();
            string email = x["email"].s();
            string password = x["password"].s();
            string country = x.has("country") ? string(x["country"].s()) : "";
            string company_size = x.has("company_size") ? string(x["company_size"].s()) : "1-10";
            string plan_code = x.has("plan_code") ? string(x["plan_code"].s()) : "starter";
            string pass_hash = sha256(password);

            pqxx::connection c(db_conn);
            pqxx::work w(c);
            auto plan = w.exec_params("SELECT id FROM pricing_plans WHERE code = $1", plan_code);
            if (plan.empty()) { crow::response res(400, "Plan not found"); add_cors(res); return res; }
            int plan_id = plan[0][0].as<int>();
            auto org = w.exec_params(
                "INSERT INTO organizations (name, slug, country, company_size, pricing_plan_id) VALUES ($1,$2,$3,$4,$5) RETURNING id",
                org_name, slug, country, company_size, plan_id);
            int org_id = org[0][0].as<int>();
            auto user = w.exec_params(
                "INSERT INTO users (organization_id, first_name, last_name, email, password_hash, role) VALUES ($1,$2,$3,$4,$5,'owner') RETURNING id",
                org_id, first_name, last_name, email, pass_hash);
            int user_id = user[0][0].as<int>();
            string token = random_token(32);
            w.exec_params("INSERT INTO sessions (user_id, token, expires_at) VALUES ($1,$2,$3::timestamp)", user_id, token, iso_now_plus_days(7));
            w.exec_params("INSERT INTO audit_logs (organization_id, user_id, action, details) VALUES ($1,$2,$3,$4::jsonb)", org_id, user_id, "auth.register", "{\"source\":\"self-service\"}");
            w.commit();

            crow::json::wvalue resj;
            resj["token"] = token;
            resj["organization_slug"] = slug;
            resj["user_role"] = "owner";
            crow::response res(200, resj); add_cors(res); return res;
        } catch (const exception& e) {
            crow::response res(500, e.what()); add_cors(res); return res;
        } });

    CROW_ROUTE(app, "/api/auth/login").methods(crow::HTTPMethod::POST)([&](const crow::request &req)
                                                                       {
        auto x = crow::json::load(req.body);
        if (!x) { crow::response res(400, "Invalid JSON"); add_cors(res); return res; }
        try {
            string email = x["email"].s();
            string password = x["password"].s();
            pqxx::connection c(db_conn);
            pqxx::work w(c);
            auto r = w.exec_params("SELECT id, password_hash, role, organization_id FROM users WHERE email = $1", email);
            if (r.empty() || r[0][1].c_str() != sha256(password)) {
                crow::response res(401, "Invalid credentials"); add_cors(res); return res;
            }
            int user_id = r[0][0].as<int>();
            string role = r[0][2].c_str();
            int org_id = r[0][3].as<int>();
            string token = random_token(32);
            w.exec_params("INSERT INTO sessions (user_id, token, expires_at) VALUES ($1,$2,$3::timestamp)", user_id, token, iso_now_plus_days(7));
            w.exec_params("INSERT INTO audit_logs (organization_id, user_id, action, details) VALUES ($1,$2,$3,$4::jsonb)", org_id, user_id, "auth.login", "{\"result\":\"ok\"}");
            w.commit();
            crow::json::wvalue resj; resj["token"] = token; resj["user_role"] = role;
            crow::response res(200, resj); add_cors(res); return res;
        } catch (const exception& e) {
            crow::response res(500, e.what()); add_cors(res); return res;
        } });

    CROW_ROUTE(app, "/api/me").methods(crow::HTTPMethod::GET)([&](const crow::request &req)
                                                              {
        try {
            pqxx::connection c(db_conn);
            auto user = require_auth(req, c);
            if (!user) { crow::response res(401); add_cors(res); return res; }
            pqxx::work w(c);
            auto r = w.exec_params(
                "SELECT u.first_name, u.last_name, u.email, u.role, o.name, o.slug, p.name, p.monthly_tokens "
                "FROM users u JOIN organizations o ON u.organization_id = o.id "
                "JOIN pricing_plans p ON o.pricing_plan_id = p.id WHERE u.id = $1", user->user_id);
            crow::json::wvalue out;
            out["first_name"] = r[0][0].c_str();
            out["last_name"] = r[0][1].c_str();
            out["email"] = r[0][2].c_str();
            out["role"] = r[0][3].c_str();
            out["organization_name"] = r[0][4].c_str();
            out["organization_slug"] = r[0][5].c_str();
            out["plan_name"] = r[0][6].c_str();
            out["monthly_tokens"] = r[0][7].as<int>();
            crow::response res(200, out); add_cors(res); return res;
        } catch (const exception& e) {
            crow::response res(500, e.what()); add_cors(res); return res;
        } });

    CROW_ROUTE(app, "/api/jira/connect").methods(crow::HTTPMethod::POST)([&](const crow::request &req)
                                                                         {
        auto x = crow::json::load(req.body);
        if (!x) { crow::response res(400); add_cors(res); return res; }
        try {
            pqxx::connection c(db_conn);
            auto user = require_auth(req, c);
            if (!user) { crow::response res(401); add_cors(res); return res; }
            pqxx::work w(c);
            w.exec_params(
                "INSERT INTO jira_connections (organization_id, base_url, user_email, api_token, project_keys, updated_at) VALUES ($1,$2,$3,$4,$5,NOW()) "
                "ON CONFLICT (organization_id) DO UPDATE SET base_url = EXCLUDED.base_url, user_email = EXCLUDED.user_email, api_token = EXCLUDED.api_token, project_keys = EXCLUDED.project_keys, updated_at = NOW()",
                user->organization_id, string(x["base_url"].s()), string(x["user_email"].s()), string(x["api_token"].s()), string(x["project_keys"].s()));
            w.exec_params("INSERT INTO audit_logs (organization_id, user_id, action, details) VALUES ($1,$2,$3,$4::jsonb)", user->organization_id, user->user_id, "jira.connect", "{\"saved\":true}");
            w.commit();
            crow::json::wvalue out; out["status"] = "saved";
            crow::response res(200, out); add_cors(res); return res;
        } catch (const exception& e) {
            crow::response res(500, e.what()); add_cors(res); return res;
        } });

    CROW_ROUTE(app, "/api/jira/sync").methods(crow::HTTPMethod::POST)([&](const crow::request &req)
                                                                      {
    try {
        pqxx::connection c(db_conn);
        auto user = require_auth(req, c);
        if (!user) {
            crow::response res(401, "Unauthorized");
            add_cors(res);
            return res;
        }

        std::string error_out;
        bool ok = sync_org_from_jira(c, user->organization_id, error_out);

        if (!ok) {
        crow::response res(
            (error_out.rfind("Jira returned HTTP 410", 0) == 0) ? 410 :
            (error_out.rfind("Jira returned HTTP 401", 0) == 0) ? 401 :
            (error_out.rfind("Jira returned HTTP 403", 0) == 0) ? 403 :
            (error_out.rfind("Jira returned HTTP 404", 0) == 0) ? 404 :
            500,
            error_out.empty() ? "Jira sync failed" : error_out
        );            
        add_cors(res);
            return res;
        }

        crow::json::wvalue out;
        out["status"] = "ok";
        out["message"] = "Jira sync completed";

        crow::response res(200, out);
        add_cors(res);
        return res;
    } catch (const std::exception& e) {
        log_line(std::string("route exception: ") + e.what());
        crow::response res(500, std::string("jira sync failed: ") + e.what());
        add_cors(res);
        return res;
    } });

    CROW_ROUTE(app, "/api/issues").methods(crow::HTTPMethod::GET)([&](const crow::request &req)
                                                                  {
        try {
            pqxx::connection c(db_conn);
            auto user = require_auth(req, c);
            if (!user) { crow::response res(401); add_cors(res); return res; }
            pqxx::work w(c);
            auto r = w.exec_params(
                "SELECT id, jira_key, jira_project_key, summary, status, priority_name, estimated_hours, actual_hours, confidence, local_updated_at "
                "FROM jira_issues "
                "WHERE organization_id = $1 "
                "ORDER BY local_updated_at DESC, id DESC "
                "LIMIT 200",
                user->organization_id
            );
            vector<crow::json::wvalue> items;
            for (const auto& row : r) {
                crow::json::wvalue it;
                it["id"] = row[0].as<int>();
                it["jira_key"] = row[1].c_str();
                it["jira_project_key"] = row[2].is_null() ? "" : row[2].c_str();
                it["summary"] = row[3].c_str();
                it["status"] = row[4].is_null() ? "" : row[4].c_str();
                it["priority_name"] = row[5].is_null() ? "" : row[5].c_str();
                it["estimated_hours"] = row[6].is_null() ? 0.0 : row[6].as<double>();
                it["actual_hours"] = row[7].is_null() ? -1.0 : row[7].as<double>();
                it["confidence"] = row[8].is_null() ? 0.0 : row[8].as<double>();
                it["updated_at"] = row[9].c_str();
                items.push_back(std::move(it));
            }
            crow::json::wvalue out; out["issues"] = std::move(items);
            crow::response res(200, out); add_cors(res); return res;
        } catch (const exception& e) {
            crow::response res(500, e.what()); add_cors(res); return res;
        } });

    CROW_ROUTE(app, "/api/issues/<int>/actual-hours").methods(crow::HTTPMethod::POST)([&](const crow::request &req, int issue_row_id)
                                                                                      {
        auto x = crow::json::load(req.body);
        if (!x) { crow::response res(400); add_cors(res); return res; }
        try {
            pqxx::connection c(db_conn);
            auto user = require_auth(req, c);
            if (!user) { crow::response res(401); add_cors(res); return res; }
            double actual_hours = x["actual_hours"].d();
            pqxx::work w(c);
            w.exec_params("UPDATE jira_issues SET actual_hours = $1, local_updated_at = NOW() WHERE id = $2 AND organization_id = $3", actual_hours, issue_row_id, user->organization_id);
            log_usage(w, user->organization_id, user->user_id, issue_row_id, "prediction", 5);
            w.commit();
            crow::json::wvalue out; out["status"] = "saved";
            crow::response res(200, out); add_cors(res); return res;
        } catch (const exception& e) {
            crow::response res(500, e.what()); add_cors(res); return res;
        } });

    CROW_ROUTE(app, "/api/dashboard").methods(crow::HTTPMethod::GET)([&](const crow::request &req)
                                                                     {
        try {
            pqxx::connection c(db_conn);
            auto user = require_auth(req, c);
            if (!user) { crow::response res(401); add_cors(res); return res; }
            pqxx::work w(c);
            auto summary = w.exec_params(
                "SELECT COUNT(*), COUNT(*) FILTER (WHERE actual_hours IS NOT NULL), COALESCE(AVG(estimated_hours),0), COALESCE(AVG(actual_hours),0) "
                "FROM jira_issues WHERE organization_id = $1", user->organization_id);
            auto tokens = w.exec_params(
                "SELECT COALESCE(SUM(tokens_used),0), (SELECT monthly_tokens FROM pricing_plans p JOIN organizations o ON o.pricing_plan_id = p.id WHERE o.id = $1) FROM token_usage WHERE organization_id = $1",
                user->organization_id);
            auto model = w.exec_params("SELECT model_type, version_name, trained_samples, created_at FROM model_versions WHERE organization_id = $1 AND is_active = TRUE ORDER BY created_at DESC LIMIT 1", user->organization_id);
            crow::json::wvalue out;
            out["total_issues"] = summary[0][0].as<int>();
            out["resolved_with_actuals"] = summary[0][1].as<int>();
            out["avg_estimated_hours"] = summary[0][2].as<double>();
            out["avg_actual_hours"] = summary[0][3].as<double>();
            int used = tokens[0][0].as<int>();
            int monthly = tokens[0][1].as<int>();
            out["tokens_used"] = used;
            out["tokens_left"] = max(0, monthly - used);
            if (!model.empty()) {
                out["active_model_type"] = model[0][0].c_str();
                out["active_model_version"] = model[0][1].c_str();
                out["active_model_samples"] = model[0][2].as<int>();
            } else {
                out["active_model_type"] = "foundation";
                out["active_model_version"] = "foundation-v1";
                out["active_model_samples"] = 0;
            }
            crow::response res(200, out); add_cors(res); return res;
        } catch (const exception& e) {
            crow::response res(500, e.what()); add_cors(res); return res;
        } });

    CROW_ROUTE(app, "/api/analytics/overview").methods(crow::HTTPMethod::GET)([&](const crow::request &req)
                                                                              {
        try {
            pqxx::connection c(db_conn);
            auto user = require_auth(req, c);
            if (!user) { crow::response res(401); add_cors(res); return res; }
            pqxx::work w(c);
            auto r = w.exec_params(
                "SELECT COUNT(*) FILTER (WHERE actual_hours IS NOT NULL), "
                "COALESCE(AVG(ABS(estimated_hours - actual_hours)),0), "
                "COALESCE(SQRT(AVG(POWER(estimated_hours - actual_hours, 2))),0), "
                "COALESCE(AVG(CASE WHEN actual_hours > 0 THEN ABS((estimated_hours - actual_hours)/actual_hours) END),0) "
                "FROM jira_issues WHERE organization_id = $1", user->organization_id);
            auto by_priority = w.exec_params(
                "SELECT priority_name, COUNT(*), COALESCE(AVG(estimated_hours),0), COALESCE(AVG(actual_hours),0) "
                "FROM jira_issues WHERE organization_id = $1 GROUP BY priority_name ORDER BY COUNT(*) DESC", user->organization_id);
            crow::json::wvalue out;
            out["samples_with_actuals"] = r[0][0].as<int>();
            out["mae"] = r[0][1].as<double>();
            out["rmse"] = r[0][2].as<double>();
            out["mape"] = r[0][3].is_null() ? 0.0 : r[0][3].as<double>();
            vector<crow::json::wvalue> groups;
            for (const auto& row : by_priority) {
                crow::json::wvalue g;
                g["priority"] = row[0].is_null() ? "Unknown" : row[0].c_str();
                g["count"] = row[1].as<int>();
                g["avg_estimated"] = row[2].as<double>();
                g["avg_actual"] = row[3].is_null() ? 0.0 : row[3].as<double>();
                groups.push_back(std::move(g));
            }
            out["by_priority"] = std::move(groups);
            crow::response res(200, out); add_cors(res); return res;
        } catch (const exception& e) {
            crow::response res(500, e.what()); add_cors(res); return res;
        } });

    CROW_ROUTE(app, "/api/billing/usage").methods(crow::HTTPMethod::GET)([&](const crow::request &req)
                                                                         {
        try {
            pqxx::connection c(db_conn);
            auto user = require_auth(req, c);
            if (!user) { crow::response res(401); add_cors(res); return res; }
            pqxx::work w(c);
            auto plan = w.exec_params(
                "SELECT p.name, p.monthly_predictions, p.monthly_tokens, p.private_model_enabled, COUNT(u.id) "
                "FROM organizations o JOIN pricing_plans p ON o.pricing_plan_id = p.id LEFT JOIN users u ON u.organization_id = o.id "
                "WHERE o.id = $1 GROUP BY p.name, p.monthly_predictions, p.monthly_tokens, p.private_model_enabled", user->organization_id);
            auto usage = w.exec_params("SELECT COALESCE(SUM(tokens_used),0), COUNT(*) FILTER (WHERE action_type='prediction'), COUNT(*) FILTER (WHERE action_type='retraining') FROM token_usage WHERE organization_id = $1", user->organization_id);
            crow::json::wvalue out;
            out["plan_name"] = plan[0][0].c_str();
            out["monthly_predictions_limit"] = plan[0][1].as<int>();
            out["monthly_tokens_limit"] = plan[0][2].as<int>();
            out["private_model_enabled"] = plan[0][3].as<bool>();
            out["users_count"] = plan[0][4].as<int>();
            out["tokens_used"] = usage[0][0].as<int>();
            out["prediction_calls"] = usage[0][1].as<int>();
            out["retraining_calls"] = usage[0][2].as<int>();
            crow::response res(200, out); add_cors(res); return res;
        } catch (const exception& e) {
            crow::response res(500, e.what()); add_cors(res); return res;
        } });

    CROW_ROUTE(app, "/api/model/status").methods(crow::HTTPMethod::GET)([&](const crow::request &req)
                                                                        {
        try {
            pqxx::connection c(db_conn);
            auto user = require_auth(req, c);
            if (!user) { crow::response res(401); add_cors(res); return res; }
            pqxx::work w(c);
            auto samples = w.exec_params("SELECT COUNT(*) FROM jira_issues WHERE organization_id = $1 AND actual_hours IS NOT NULL", user->organization_id);
            auto active = w.exec_params("SELECT model_type, version_name, model_path, trained_samples, mae, rmse, created_at FROM model_versions WHERE organization_id = $1 AND is_active = TRUE ORDER BY created_at DESC LIMIT 1", user->organization_id);
            crow::json::wvalue out;
            out["actual_samples"] = samples[0][0].as<int>();
            out["retrain_eligible"] = samples[0][0].as<int>() >= 10;
            if (!active.empty()) {
                out["model_type"] = active[0][0].c_str();
                out["version_name"] = active[0][1].c_str();
                out["model_path"] = active[0][2].c_str();
                out["trained_samples"] = active[0][3].as<int>();
                out["mae"] = active[0][4].is_null() ? 0.0 : active[0][4].as<double>();
                out["rmse"] = active[0][5].is_null() ? 0.0 : active[0][5].as<double>();
            } else {
                out["model_type"] = "foundation";
                out["version_name"] = "foundation-v1";
                out["model_path"] = env_or("MODEL_DIR", "/app/model") + "/foundation_task_estimation_model.json";
                out["trained_samples"] = 0;
                out["mae"] = 0.0;
                out["rmse"] = 0.0;
            }
            crow::response res(200, out); add_cors(res); return res;
        } catch (const exception& e) {
            crow::response res(500, e.what()); add_cors(res); return res;
        } });

    CROW_ROUTE(app, "/api/model/retrain").methods(crow::HTTPMethod::POST)([&](const crow::request &req)
                                                                          {
        try {
            pqxx::connection c(db_conn);
            auto user = require_auth(req, c);
            if (!user) { crow::response res(401); add_cors(res); return res; }
            {
                pqxx::work w(c);
                auto plan = w.exec_params(
                    "SELECT p.private_model_enabled FROM organizations o JOIN pricing_plans p ON o.pricing_plan_id = p.id WHERE o.id = $1",
                    user->organization_id);
                if (plan.empty() || !plan[0][0].as<bool>()) {
                    crow::response res(403, "Current plan does not include private retraining"); add_cors(res); return res;
                }
                auto samples = w.exec_params("SELECT COUNT(*) FROM jira_issues WHERE organization_id = $1 AND actual_hours IS NOT NULL", user->organization_id);
                if (samples[0][0].as<int>() < 10) {
                    crow::response res(400, "Need at least 10 resolved issues with actual hours"); add_cors(res); return res;
                }
                w.exec_params("INSERT INTO training_jobs (organization_id, status, started_at) VALUES ($1, 'running', NOW())", user->organization_id);
                log_usage(w, user->organization_id, user->user_id, {}, "retraining", 500);
                w.commit();
            }

            string cmd = "python3 /app/training/retrain_from_db.py --organization-id " + to_string(user->organization_id);
            int rc = std::system(cmd.c_str());

            pqxx::work w2(c);
            if (rc == 0) {
                w2.exec_params("UPDATE training_jobs SET status='completed', finished_at=NOW(), logs='ok' WHERE organization_id=$1 AND status='running'", user->organization_id);
            } else {
                w2.exec_params("UPDATE training_jobs SET status='failed', finished_at=NOW(), logs=$2 WHERE organization_id=$1 AND status='running'", user->organization_id, string("exit_code=") + to_string(rc));
            }
            w2.commit();

            crow::json::wvalue out; out["status"] = rc == 0 ? "completed" : "failed";
            crow::response res(rc == 0 ? 200 : 500, out); add_cors(res); return res;
        } catch (const exception& e) {
            crow::response res(500, e.what()); add_cors(res); return res;
        } });

    CROW_ROUTE(app, "/api/webhook/jira/<string>").methods(crow::HTTPMethod::POST)([&](const crow::request &req, const string &org_slug)
                                                                                  {
    auto payload = crow::json::load(req.body);
    if (!payload) {
        crow::response res(400, "Invalid JSON");
        add_cors(res);
        return res;
    }

    try {
        pqxx::connection c(db_conn);
        pqxx::work w(c);

        auto org = w.exec_params("SELECT id FROM organizations WHERE slug = $1", org_slug);
        if (org.empty()) {
            crow::response res(404, "Organization not found");
            add_cors(res);
            return res;
        }

        int organization_id = org[0][0].as<int>();

        if (!payload.has("issue")) {
            crow::response res(400, "Missing issue payload");
            add_cors(res);
            return res;
        }

        const auto& issue = payload["issue"];
        string webhook_event =
            (payload.has("webhookEvent") && payload["webhookEvent"].t() == crow::json::type::String)
            ? string(payload["webhookEvent"].s())
            : "";

        sync_single_issue(w, organization_id, issue);

        w.exec_params(
            "INSERT INTO audit_logs (organization_id, action, details) VALUES ($1,$2,$3::jsonb)",
            organization_id,
            "jira.webhook",
            "{\"processed\":true}"
        );
        w.commit();

        if (webhook_event == "jira:issue_created") {
            string summary =
                (issue.has("fields") && issue["fields"].has("summary") && issue["fields"]["summary"].t() == crow::json::type::String)
                ? string(issue["fields"]["summary"].s()) : "";

            string description = "";
            if (issue.has("fields") && issue["fields"].has("description") && issue["fields"]["description"].t() != crow::json::type::Null) {
                description = flatten_adf(issue["fields"]["description"]);
            }

            string priority_name = "Medium";
            if (issue.has("fields") &&
                issue["fields"].has("priority") &&
                issue["fields"]["priority"].t() == crow::json::type::Object &&
                issue["fields"]["priority"].has("name") &&
                issue["fields"]["priority"]["name"].t() == crow::json::type::String) {
                priority_name = string(issue["fields"]["priority"]["name"].s());
            }

            int priority_code = map_priority_code(priority_name);
            int role_code = 1;
            auto prediction = predict_hours_for_org(organization_id, priority_code, role_code, summary + " " + description);

            string issue_key =
                (issue.has("key") && issue["key"].t() == crow::json::type::String)
                ? string(issue["key"].s()) : "";

            if (!issue_key.empty()) {
                string comment_error;
                if (!send_jira_comment_for_org(c, organization_id, issue_key, prediction.hours, comment_error)) {
                    log_line("comment failed for " + issue_key + ": " + comment_error);
                } else {
                    log_line("comment posted for " + issue_key);
                }
            }
        }

        crow::json::wvalue out;
        out["status"] = "processed";
        crow::response res(200, out);
        add_cors(res);
        return res;
    } catch (const exception& e) {
        crow::response res(500, e.what());
        add_cors(res);
        return res;
    } });

    std::cout << "Backend listening on port " << port << std::endl;
    app.port(port).multithreaded().run();
    curl_global_cleanup();
}
