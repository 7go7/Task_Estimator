import xgboost as xgb
import numpy as np
import os
import json
from sklearn.feature_extraction.text import TfidfVectorizer

# 1. Expanded "raw" data coming from the frontend
raw_data = [
    # Junior (Role: 0)
    {"title": "Fix button", "desc": "Make it blue", "priority": 0, "role": 0, "actual_hours": 1.5},
    {"title": "Typo in footer", "desc": "Change 'Copright' to 'Copyright'", "priority": 0, "role": 0, "actual_hours": 0.5},
    {"title": "Update README", "desc": "Add instructions for local Docker setup.", "priority": 1, "role": 0, "actual_hours": 2.0},
    {"title": "CSS Alignment", "desc": "Center the login form on mobile devices.", "priority": 1, "role": 0, "actual_hours": 3.0},
    {"title": "Image compression", "desc": "Compress the hero images on the landing page to improve loading speed.", "priority": 1, "role": 0, "actual_hours": 2.5},
    {"title": "Simple API fetch", "desc": "Call the weather API and log the result to the console.", "priority": 0, "role": 0, "actual_hours": 4.0},
    {"title": "Update NPM packages", "desc": "Run npm update and fix any minor breaking changes in the build.", "priority": 1, "role": 0, "actual_hours": 5.0},
    {"title": "Add tooltip", "desc": "Add a descriptive tooltip over the settings icon.", "priority": 0, "role": 0, "actual_hours": 1.0},
    {"title": "Color palette update", "desc": "Change the primary brand color across all SCSS variables.", "priority": 1, "role": 0, "actual_hours": 3.5},
    {"title": "Write unit tests", "desc": "Write basic Jest tests for the string utility functions.", "priority": 2, "role": 0, "actual_hours": 6.0},
    {"title": "Fix 404 page", "desc": "Add a return home button to the 404 error page.", "priority": 0, "role": 0, "actual_hours": 1.5},
    {"title": "Create mock data", "desc": "Generate a JSON file with 50 fake users for testing.", "priority": 0, "role": 0, "actual_hours": 2.0},
    {"title": "Console error cleanup", "desc": "Remove all unused variables causing warnings in the browser console.", "priority": 1, "role": 0, "actual_hours": 4.0},
    {"title": "Add favicon", "desc": "Upload and link the new company logo as the site favicon.", "priority": 0, "role": 0, "actual_hours": 0.5},
    {"title": "Form validation", "desc": "Ensure the email input field requires an @ symbol before submitting.", "priority": 2, "role": 0, "actual_hours": 5.0},
    {"title": "Update dependencies", "desc": "Bump minor versions of UI library.", "priority": 1, "role": 0, "actual_hours": 3.0},
    {"title": "Translation tags", "desc": "Wrap hardcoded English text in i18n translation functions.", "priority": 1, "role": 0, "actual_hours": 6.0},
    {"title": "Hover effect", "desc": "Make table rows highlight gray when hovered.", "priority": 0, "role": 0, "actual_hours": 1.0},

    # Middle (Role: 1)
    {"title": "API Endpoint", "desc": "Create GET /users endpoint with pagination and JWT auth", "priority": 2, "role": 1, "actual_hours": 5.0},
    {"title": "Stripe Integration", "desc": "Implement a basic checkout session for the standard tier subscription.", "priority": 3, "role": 1, "actual_hours": 14.0},
    {"title": "Email Notifications", "desc": "Set up SendGrid to fire welcome emails when a user registers.", "priority": 2, "role": 1, "actual_hours": 6.0},
    {"title": "Data Export", "desc": "Allow admins to export the filtered user list as a CSV file.", "priority": 1, "role": 1, "actual_hours": 8.0},
    {"title": "Password Reset", "desc": "Build the forgot password flow including token generation and validation.", "priority": 3, "role": 1, "actual_hours": 12.0},
    {"title": "Dashboard Charts", "desc": "Integrate Chart.js to display monthly revenue and active users.", "priority": 2, "role": 1, "actual_hours": 10.0},
    {"title": "File Upload", "desc": "Create a drag-and-drop zone that uploads profile pictures to AWS S3.", "priority": 2, "role": 1, "actual_hours": 9.0},
    {"title": "Refactor Navigation", "desc": "Rewrite the sidebar menu to support infinite nesting levels.", "priority": 1, "role": 1, "actual_hours": 7.0},
    {"title": "Rate Limiting", "desc": "Add Redis-based rate limiting to the public API endpoints to prevent spam.", "priority": 3, "role": 1, "actual_hours": 8.0},
    {"title": "Role Middleware", "desc": "Create Express middleware to restrict routes based on user roles.", "priority": 2, "role": 1, "actual_hours": 5.5},
    {"title": "Search Bar", "desc": "Implement a fuzzy search for the product catalog using the database.", "priority": 2, "role": 1, "actual_hours": 11.0},
    {"title": "Bug: Double submit", "desc": "Disable the submit button and show a spinner while API request is pending.", "priority": 3, "role": 1, "actual_hours": 3.0},
    {"title": "Webhook handler", "desc": "Receive payment success events from Stripe and update database status.", "priority": 3, "role": 1, "actual_hours": 9.0},
    {"title": "Cache responses", "desc": "Cache the global settings API response in memory for 15 minutes.", "priority": 1, "role": 1, "actual_hours": 4.0},
    {"title": "Dark Mode Toggle", "desc": "Implement context-based theme switching and persist user preference in localStorage.", "priority": 1, "role": 1, "actual_hours": 8.0},
    {"title": "Multi-select component", "desc": "Build a custom dropdown that allows selecting multiple tags with auto-complete.", "priority": 2, "role": 1, "actual_hours": 10.0},
    {"title": "Audit Logs UI", "desc": "Build a timeline view to display user activity logs.", "priority": 1, "role": 1, "actual_hours": 6.0},
    {"title": "PDF Generation", "desc": "Generate invoice PDFs on the server using Puppeteer.", "priority": 2, "role": 1, "actual_hours": 12.0},
    
    # Senior (Role: 2)
    {"title": "DB Schema", "desc": "Design and normalize PostgreSQL database with 15 tables and relations. Write migrations.", "priority": 3, "role": 2, "actual_hours": 12.0},
    {"title": "Microservices Sync", "desc": "Design an event-driven Kafka architecture to synchronize user states across 3 distinct microservices.", "priority": 3, "role": 2, "actual_hours": 30.0},
    {"title": "Security Audit", "desc": "Review application for OWASP top 10 vulnerabilities and patch XSS vectors.", "priority": 3, "role": 2, "actual_hours": 18.0},
    {"title": "CI/CD Pipeline", "desc": "Set up GitHub Actions to run tests, build Docker images, and deploy to staging.", "priority": 2, "role": 2, "actual_hours": 16.0},
    {"title": "Memory Leak Debug", "desc": "Profile the Node.js production server to find and fix a severe memory leak crashing the app.", "priority": 3, "role": 2, "actual_hours": 24.0},
    {"title": "Elasticsearch Setup", "desc": "Deploy an Elasticsearch cluster and sync it with the main relational database.", "priority": 2, "role": 2, "actual_hours": 26.0},
    {"title": "Zero-Downtime Migration", "desc": "Migrate 5 million legacy user records to the new schema without interrupting live traffic.", "priority": 3, "role": 2, "actual_hours": 35.0},
    {"title": "GraphQL Implementation", "desc": "Introduce a GraphQL federation layer over the existing REST APIs.", "priority": 2, "role": 2, "actual_hours": 40.0},
    {"title": "Optimize SQL Queries", "desc": "Analyze query execution plans and add compound indexes to reduce load times by 50%.", "priority": 2, "role": 2, "actual_hours": 10.0},
    {"title": "Infrastructure as Code", "desc": "Write Terraform scripts to provision AWS VPC, RDS, and ECS clusters.", "priority": 3, "role": 2, "actual_hours": 22.0},
    {"title": "OAuth 2.0 Server", "desc": "Implement a custom OAuth 2.0 provider to allow third-party apps to integrate.", "priority": 3, "role": 2, "actual_hours": 28.0},
    {"title": "WebSockets Scaling", "desc": "Architect a Redis Pub/Sub system to scale WebSockets across multiple server instances.", "priority": 3, "role": 2, "actual_hours": 15.0},
    {"title": "Mentorship & Code Review", "desc": "Conduct a deep dive architectural review of the mid-level developer PRs.", "priority": 1, "role": 2, "actual_hours": 5.0},
    {"title": "Disaster Recovery", "desc": "Implement automated cross-region database backups and document the recovery plan.", "priority": 3, "role": 2, "actual_hours": 14.0},
    {"title": "Server Side Rendering", "desc": "Migrate the React SPA to Next.js for better SEO and initial load times.", "priority": 2, "role": 2, "actual_hours": 32.0},
    {"title": "Vendor Lock-in Removal", "desc": "Abstract the cloud storage layer so the app can switch from AWS S3 to Google Cloud Storage dynamically.", "priority": 1, "role": 2, "actual_hours": 20.0}
]

print("1. Ініціалізація словника (NLP Vectorization)...")
descriptions = [item["desc"] for item in raw_data]

# Використовуємо TF-IDF для створення базового словника (максимум 25 слів для старту)
vectorizer = TfidfVectorizer(max_features=25, stop_words='english')
X_text_features = vectorizer.fit_transform(descriptions).toarray()

vocabulary = vectorizer.get_feature_names_out().tolist()
print(f"Стартовий словник ({len(vocabulary)} слів): {vocabulary}")

# 2. Формуємо фінальну матрицю ознак
# ВАЖЛИВО: C++ сервер зараз очікує вектор суворо у форматі: [priority, role, word1, word2, ... wordN]
X_train = []
y_train = []

for i, item in enumerate(raw_data):
    base_features = [item["priority"], item["role"]]
    text_features = X_text_features[i].tolist()
    
    # З'єднуємо пріоритет, роль та масив частоти слів
    X_train.append(base_features + text_features)
    y_train.append(item["actual_hours"])

X_train = np.array(X_train)
y_train = np.array(y_train)

# 3. Навчання стартової моделі
print(f"2. Тренування базової моделі на {len(X_train)} прикладах...")
model = xgb.XGBRegressor(
    objective='reg:squarederror', 
    n_estimators=100,      # Збільшено кількість дерев для кращої точності
    learning_rate=0.1,     # Швидкість навчання
    max_depth=4            # Глибина дерев
)
model.fit(X_train, y_train)

# 4. Збереження словника та моделі для C++
# Шлях вказує на папку build/model відносно папки training
save_dir = '../backend/build/model'
os.makedirs(save_dir, exist_ok=True)

# Збереження моделі
model_path = os.path.join(save_dir, 'task_estimation_model.json')
model.save_model(model_path)

# Збереження словника (vocabulary.json)
vocab_path = os.path.join(save_dir, 'vocabulary.json')
with open(vocab_path, 'w') as f:
    json.dump(vocabulary, f)

print(f"Готово! Модель збережена у: {model_path}")
print(f"Словник збережено у: {vocab_path}")