import pandas as pd
import xgboost as xgb
import os
from sqlalchemy import create_engine
from hasher import vectorize_text_hashed

engine = create_engine('postgresql://admin:secretpassword@localhost:5432/task_estimator')
df = pd.read_sql("SELECT description, priority_code, role_code, actual_hours FROM Tasks WHERE actual_hours IS NOT NULL", engine)

if len(df) < 3:
    print("Недостатньо даних")
    exit()

X_new = []
for i in range(len(df)):
    text_vector = vectorize_text_hashed(df.iloc[i]['description'])
    X_new.append([df.iloc[i]['priority_code'], df.iloc[i]['role_code']] + text_vector)

y_new = df['actual_hours'].values

model_path = '../backend/build/model/task_estimation_model.json'
model = xgb.XGBRegressor()
model.load_model(model_path)

print("Інкрементальне донавчання на захешованих ознаках...")
model.fit(X_new, y_new, xgb_model=model_path)
model.save_model(model_path)
print("Готово!")