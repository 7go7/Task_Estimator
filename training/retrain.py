import pandas as pd
import xgboost as xgb
from sqlalchemy import create_engine
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_absolute_error
import os

# 1. Налаштування підключення до БД PostgreSQL (ті самі, що в docker-compose)
DB_USER = 'admin'
DB_PASS = 'secretpassword'
DB_HOST = 'localhost'
DB_PORT = '5432'
DB_NAME = 'task_estimator'

# Шлях, куди зберігати оновлену модель (щоб C++ міг її підхопити)
MODEL_SAVE_PATH = '../backend/model/task_estimation_model.json'

def retrain_model():
    print("Підключення до бази даних...")
    engine = create_engine(f'postgresql://{DB_USER}:{DB_PASS}@{DB_HOST}:{DB_PORT}/{DB_NAME}')
    
    # 2. Витягуємо дані для навчання
    # Беремо лише ті задачі, які завершені і мають реальний фактичний час (actual_hours)
    # Примітка: Тобі треба буде додати колонки priority_code, role_code, actual_hours у таблицю Tasks
    query = """
        SELECT description, priority_code, role_code, actual_hours 
        FROM Tasks 
        WHERE actual_hours IS NOT NULL
    """
    df = pd.read_sql(query, engine)
    
    if len(df) < 50:
        print(f"Недостатньо даних для перенавчання (знайдено {len(df)} записів. Мінімум 50).")
        return

    print(f"Знайдено {len(df)} завершених задач. Підготовка даних...")
    
    # Обчислюємо довжину опису (як у C++ бекенді)
    df['description_len'] = df['description'].fillna('').apply(len)
    
    # Формуємо матрицю ознак X (порядок має збігатися з C++ кодом!)
    # У C++ ми робили: priority_code, role_code, description_len
    X = df[['priority_code', 'role_code', 'description_len']]
    y = df['actual_hours']

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.15, random_state=42)

    print("Початок навчання моделі XGBoost...")
    # Налаштування параметрів (можеш підібрати їх на основі свого першого експерименту)
    model = xgb.XGBRegressor(
        objective='reg:squarederror', 
        n_estimators=150,
        learning_rate=0.1,
        max_depth=5
    )
    
    model.fit(X_train, y_train)

    # Оцінка якості (MAE)
    predictions = model.predict(X_test)
    mae = mean_absolute_error(y_test, predictions)
    print(f"Нова модель натренована! Mean Absolute Error (MAE): {mae:.2f} годин")

    # 3. Збереження моделі
    # Перевіряємо, чи існує папка
    os.makedirs(os.path.dirname(MODEL_SAVE_PATH), exist_ok=True)
    
    # Використовуємо формат JSON, бо він підтримується C API XGBoost
    model.save_model(MODEL_SAVE_PATH)
    print(f"Модель успішно збережена у: {MODEL_SAVE_PATH}")
    print("ВАЖЛИВО: Перезапусти C++ Backend або реалізуй API для гарячого перезавантаження моделі!")

if __name__ == "__main__":
    retrain_model()