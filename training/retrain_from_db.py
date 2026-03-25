import argparse
import os
import pathlib
from datetime import datetime

import pandas as pd
import xgboost as xgb
from sqlalchemy import create_engine, text
from sklearn.metrics import mean_absolute_error, mean_squared_error

from hasher import vectorize_text_hashed


def build_db_url() -> str:
    explicit = os.getenv("DATABASE_URL")
    if explicit:
        return explicit

    host = os.getenv("POSTGRES_HOST", "db")
    port = os.getenv("POSTGRES_PORT", "5432")
    db = os.getenv("POSTGRES_DB", "task_estimator")
    user = os.getenv("POSTGRES_USER", "admin")
    password = os.getenv("POSTGRES_PASSWORD", "secretpassword")

    return f"postgresql+psycopg2://{user}:{password}@{host}:{port}/{db}"


def safe_text(value) -> str:
    if value is None:
        return ""
    try:
        if pd.isna(value):
            return ""
    except Exception:
        pass
    return str(value)


def build_features(df: pd.DataFrame):
    X = []
    for _, row in df.iterrows():
        txt = f"{safe_text(row.get('summary'))} {safe_text(row.get('description'))}".strip()
        X.append(
            [int(row["priority_code"]), int(row["role_code"])]
            + vectorize_text_hashed(txt)
        )
    return X


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--organization-id", type=int, required=True)
    args = parser.parse_args()

    db_url = build_db_url()
    model_dir = pathlib.Path(os.getenv("MODEL_DIR", "/app/model"))
    model_dir.mkdir(parents=True, exist_ok=True)

    foundation_path = model_dir / "foundation_task_estimation_model.json"
    tenant_path = model_dir / f"org_{args.organization_id}_task_estimation_model.json"

    if not foundation_path.exists():
        raise SystemExit(f"Foundation model not found: {foundation_path}")

    engine = create_engine(db_url)

    query = text(
        """
        SELECT summary, description, priority_code, role_code, actual_hours
        FROM jira_issues
        WHERE organization_id = :organization_id
          AND actual_hours IS NOT NULL
        ORDER BY id
        """
    )

    df = pd.read_sql(query, engine, params={"organization_id": args.organization_id})

    if len(df) < 10:
        raise SystemExit("Need at least 10 resolved issues with actual hours")

    X = build_features(df)
    y = df["actual_hours"].astype(float).tolist()

    booster_path = str(tenant_path if tenant_path.exists() else foundation_path)

    model = xgb.XGBRegressor(
        n_estimators=80,
        max_depth=6,
        learning_rate=0.08,
        subsample=0.9,
        colsample_bytree=0.9,
        objective="reg:squarederror",
        random_state=42,
        n_jobs=4,
    )

    model.fit(X, y, xgb_model=booster_path if os.path.exists(booster_path) else None)
    model.save_model(tenant_path)

    preds = model.predict(X)
    mae = float(mean_absolute_error(y, preds))
    rmse = float(mean_squared_error(y, preds, squared=False))

    version_name = f"org-{args.organization_id}-private-{datetime.utcnow().strftime('%Y%m%d%H%M%S')}"

    with engine.begin() as conn:
        conn.execute(
            text("UPDATE model_versions SET is_active = FALSE WHERE organization_id = :org_id"),
            {"org_id": args.organization_id},
        )
        conn.execute(
            text(
                """
                INSERT INTO model_versions
                    (organization_id, model_type, version_name, model_path, trained_samples, mae, rmse, is_active)
                VALUES
                    (:org_id, :model_type, :version_name, :model_path, :trained_samples, :mae, :rmse, TRUE)
                """
            ),
            {
                "org_id": args.organization_id,
                "model_type": "private",
                "version_name": version_name,
                "model_path": str(tenant_path),
                "trained_samples": int(len(df)),
                "mae": mae,
                "rmse": rmse,
            },
        )

    print(f"Tenant model saved: {tenant_path}")
    print(f"Samples: {len(df)} | MAE: {mae:.4f} | RMSE: {rmse:.4f}")


if __name__ == "__main__":
    main()