-- 1. Таблиця Users
-- Містить інформацію про всіх користувачів системи[cite: 374].
CREATE TABLE Users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(255) NOT NULL,
    email VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    role VARCHAR(50) NOT NULL CHECK (role IN ('administrator', 'manager', 'developer')) 
);

-- 2. Таблиця Sprints
-- Сутність для планування ітерацій[cite: 380].
CREATE TABLE Sprints (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    goal TEXT,
    start_date DATE,
    end_date DATE
);

-- 3. Таблиця Tasks
-- Центральна сутність системи. Зберігає зв'язок із Jira[cite: 376, 377].
CREATE TABLE Tasks (
    id SERIAL PRIMARY KEY,
    title VARCHAR(255) NOT NULL,
    description TEXT,
    status VARCHAR(50) DEFAULT 'To Do',
    priority VARCHAR(50),
    task_type VARCHAR(50),
    assignee_id INT REFERENCES Users(id) ON DELETE SET NULL,
    creator_id INT REFERENCES Users(id) ON DELETE SET NULL,
    sprint_id INT REFERENCES Sprints(id) ON DELETE SET NULL,
    jira_key VARCHAR(50) UNIQUE, 
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 4. Таблиця Estimates
-- Зберігає результати оцінювання: прогнозоване значення ML та ручну оцінку.
CREATE TABLE Estimates (
    id SERIAL PRIMARY KEY,
    task_id INT REFERENCES Tasks(id) ON DELETE CASCADE,
    ml_value FLOAT,
    manual_value FLOAT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 5. Асоціативна таблиця Task_Sprint 
-- Для реалізації зв'язку «багато до багатьох» (задача може переходити з одного спринту в інший)[cite: 381].
CREATE TABLE Task_Sprint (
    task_id INT REFERENCES Tasks(id) ON DELETE CASCADE,
    sprint_id INT REFERENCES Sprints(id) ON DELETE CASCADE,
    PRIMARY KEY (task_id, sprint_id)
);