CREATE TABLE IF NOT EXISTS pricing_plans (
    id SERIAL PRIMARY KEY,
    code VARCHAR(50) UNIQUE NOT NULL,
    name VARCHAR(100) NOT NULL,
    max_users INT,
    monthly_predictions INT,
    monthly_tokens INT,
    private_model_enabled BOOLEAN NOT NULL DEFAULT FALSE,
    jira_sync_enabled BOOLEAN NOT NULL DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS organizations (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    slug VARCHAR(255) UNIQUE NOT NULL,
    country VARCHAR(100),
    company_size VARCHAR(50),
    pricing_plan_id INT REFERENCES pricing_plans(id),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    organization_id INT NOT NULL REFERENCES organizations(id) ON DELETE CASCADE,
    first_name VARCHAR(100) NOT NULL,
    last_name VARCHAR(100) NOT NULL,
    email VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    role VARCHAR(50) NOT NULL CHECK (role IN ('owner', 'manager', 'developer', 'viewer')),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS sessions (
    id SERIAL PRIMARY KEY,
    user_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    token VARCHAR(128) UNIQUE NOT NULL,
    expires_at TIMESTAMP NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS jira_connections (
    id SERIAL PRIMARY KEY,
    organization_id INT NOT NULL UNIQUE REFERENCES organizations(id) ON DELETE CASCADE,
    base_url TEXT NOT NULL,
    user_email VARCHAR(255) NOT NULL,
    api_token TEXT NOT NULL,
    project_keys TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS jira_projects (
    id SERIAL PRIMARY KEY,
    organization_id INT NOT NULL REFERENCES organizations(id) ON DELETE CASCADE,
    jira_project_key VARCHAR(50) NOT NULL,
    jira_project_name VARCHAR(255),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (organization_id, jira_project_key)
);

CREATE TABLE IF NOT EXISTS jira_issues (
    id SERIAL PRIMARY KEY,
    organization_id INT NOT NULL REFERENCES organizations(id) ON DELETE CASCADE,
    jira_issue_id VARCHAR(64),
    jira_key VARCHAR(64) NOT NULL,
    jira_project_key VARCHAR(50),
    summary TEXT NOT NULL,
    description TEXT,
    issue_type VARCHAR(100),
    status VARCHAR(100),
    priority_name VARCHAR(100),
    priority_code INT NOT NULL DEFAULT 1,
    assignee_name VARCHAR(255),
    role_code INT NOT NULL DEFAULT 1,
    story_points NUMERIC(10,2),
    estimated_hours NUMERIC(10,2),
    actual_hours NUMERIC(10,2),
    confidence NUMERIC(10,4),
    source_updated_at TIMESTAMP,
    local_updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (organization_id, jira_key)
);

CREATE TABLE IF NOT EXISTS model_versions (
    id SERIAL PRIMARY KEY,
    organization_id INT REFERENCES organizations(id) ON DELETE CASCADE,
    model_type VARCHAR(50) NOT NULL CHECK (model_type IN ('foundation', 'private')),
    version_name VARCHAR(100) NOT NULL,
    model_path TEXT NOT NULL,
    trained_samples INT DEFAULT 0,
    mae NUMERIC(10,4),
    rmse NUMERIC(10,4),
    is_active BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS training_jobs (
    id SERIAL PRIMARY KEY,
    organization_id INT NOT NULL REFERENCES organizations(id) ON DELETE CASCADE,
    status VARCHAR(50) NOT NULL CHECK (status IN ('queued', 'running', 'completed', 'failed')),
    logs TEXT,
    started_at TIMESTAMP,
    finished_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS token_usage (
    id SERIAL PRIMARY KEY,
    organization_id INT NOT NULL REFERENCES organizations(id) ON DELETE CASCADE,
    user_id INT REFERENCES users(id) ON DELETE SET NULL,
    jira_issue_id INT REFERENCES jira_issues(id) ON DELETE SET NULL,
    action_type VARCHAR(50) NOT NULL CHECK (action_type IN ('prediction', 'retraining', 'jira_sync')),
    tokens_used INT NOT NULL DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS audit_logs (
    id SERIAL PRIMARY KEY,
    organization_id INT REFERENCES organizations(id) ON DELETE CASCADE,
    user_id INT REFERENCES users(id) ON DELETE SET NULL,
    action VARCHAR(100) NOT NULL,
    details JSONB,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO pricing_plans (code, name, max_users, monthly_predictions, monthly_tokens, private_model_enabled, jira_sync_enabled)
VALUES
('starter', 'Starter', 5, 500, 5000, FALSE, TRUE),
('team', 'Team', 20, 5000, 50000, TRUE, TRUE),
('business', 'Business', 100, 25000, 250000, TRUE, TRUE),
('enterprise', 'Enterprise', 1000, 999999, 9999999, TRUE, TRUE)
ON CONFLICT (code) DO NOTHING;
