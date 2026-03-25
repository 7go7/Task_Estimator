# AI Task Estimator SaaS (React + C++/Crow + PostgreSQL + XGBoost + Python + Jira)

A diploma-oriented MVP built around the original repo structure and stack. It keeps ML as the core of the system and uses Jira as the task source of truth instead of implementing a separate custom board.

## What this project does

- Company registration with tariff selection
- Login and bearer-session auth
- Multi-tenant isolation by `organization_id`
- Jira integration using Atlassian REST API and webhook receiver
- Jira issue mirroring into PostgreSQL for analytics and safe tenant-local training
- C++ inference service using XGBoost C API
- Python foundation-model training and tenant-private retraining
- Dashboard, analytics, model status, and token usage in React
- Per-organization private model file trained only from that organization's resolved issues

## Architecture

- **Frontend**: React + Vite
- **Backend**: C++17 + Crow + libpqxx + libcurl + XGBoost C API
- **Database**: PostgreSQL
- **ML training**: Python + xgboost + SQLAlchemy
- **Task source**: Jira Cloud API + webhooks

## Important design decision

This project does **not** implement a separate Kanban board clone. Jira stays the source of truth for projects/issues. The platform stores a synchronized tenant-local mirror for:

- ML inference
- analytics
- actual hours feedback
- private retraining
- quota tracking

## Run with Docker

### 1. Copy environment file

```bash
cp .env.example .env
```

Fill these variables:

- `APP_URL`
- `CORS_ORIGIN`
- `POSTGRES_*`
- `JIRA_APP_USER_EMAIL`
- `JIRA_APP_API_TOKEN`

For local testing you can leave Jira credentials empty and use the seed/demo mode.

### 2. Build and start

```bash
docker compose up --build
```

### 3. Open the app

- Frontend: `http://localhost:5173`
- Backend API: `http://localhost:8080`

## Default pricing plans

- Starter
- Team
- Business
- Enterprise

These are seeded by `database/init.sql`.

## Demo flow

1. Register a company and choose a tariff plan
2. Login
3. Connect Jira in **Integrations**
4. Sync issues from Jira project(s)
5. Open **Issues** and see ML estimates
6. Set actual hours for completed issues
7. Open **Model** and trigger retraining
8. Review **Analytics** and **Billing**

## Jira setup

### OAuth vs API token
This MVP uses Atlassian user email + API token for simplicity.

### How to connect
In the UI, save:

- Atlassian site base URL, for example `https://your-domain.atlassian.net`
- Cloud email
- API token
- One or more Jira project keys, for example `ABC,PLATFORM`

### Webhook URL
Create a Jira webhook pointing to:

```text
http://localhost:8080/api/webhook/jira/<your-organization-slug>
```

If running publicly, replace with your public backend URL.

Recommended events:
- issue created
- issue updated
- issue deleted

## ML lifecycle

### Foundation model
`training/train_foundation.py` creates the initial model using synthetic seed data similar to your original repository.

### Per-tenant retraining
`training/retrain_from_db.py`:
- loads resolved issues for one organization only
- continues training from the current foundation or active tenant model
- saves `backend/model/org_<ORG_ID>_task_estimation_model.json`
- records a new `model_versions` row

### Inference
The C++ backend:
- loads the foundation model at startup
- for each organization, tries to use an active tenant model if it exists
- falls back to the foundation model otherwise

## Local development without Docker

### Backend
You need:
- CMake
- g++ with C++17
- PostgreSQL client headers
- libpqxx
- libcurl
- XGBoost shared library and headers

Then:

```bash
cd backend
mkdir -p build && cd build
cmake ..
cmake --build .
./task_estimator_backend
```

### Frontend

```bash
cd frontend
npm install
npm run dev
```

### Training

```bash
cd training
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python train_foundation.py
```

## Notes

- This is a diploma-ready MVP, not a finished enterprise product.
- Auth uses opaque bearer sessions stored in PostgreSQL to keep the C++ backend simpler.
- Replacing sessions with JWT later is straightforward.
- Secrets should come from environment variables only.


## Build fix for XGBoost
If Docker fails with an error about `dmlc-core` missing during the backend build, the backend Dockerfile must clone XGBoost recursively with submodules. This archive already includes that fix.


## v3 patch
- Frontend no longer forces `Content-Type: application/json` on GET requests.
- Backend now defaults `CORS_ORIGIN=*` and sends broader preflight headers.


## Ngrok testing

If you expose the frontend with ngrok, for example on port 5173, open the app using the ngrok URL. The Jira Connection page will automatically display the correct webhook URL based on the current browser origin.

Example:

```bash
ngrok http 5173
```

Then open the provided `https://...ngrok...` URL in your browser and copy the webhook shown on the Jira page.

If you ever need a fixed public URL instead of auto-detection, set `VITE_PUBLIC_BASE_URL` before building the frontend.
