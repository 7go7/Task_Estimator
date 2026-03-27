import React, { useEffect, useMemo, useState } from 'react';

const API = import.meta.env.VITE_API_URL || '/api';

const PUBLIC_BASE_URL = (import.meta.env.VITE_PUBLIC_BASE_URL || (typeof window !== 'undefined' ? window.location.origin : '') || '').replace(/\/$/, '');

function buildWebhookUrl(orgSlug) {
  return `${PUBLIC_BASE_URL}/api/webhook/jira/${orgSlug}`;
}

function authHeaders(token) {
  return token ? { Authorization: `Bearer ${token}` } : {};
}

function parseCsv(value = '') {
  return value
    .split(',')
    .map((item) => item.trim())
    .filter(Boolean);
}

function uniq(values) {
  return [...new Set(values.filter(Boolean))];
}

async function api(path, { method = 'GET', token, body } = {}) {
  const headers = {
    ...authHeaders(token),
  };
  if (body !== undefined) headers['Content-Type'] = 'application/json';

  let res;
  try {
    const url = path.startsWith('/api') ? `${API}${path.slice(4)}` : `${API}${path}`;
    res = await fetch(url, {
      method,
      headers,
      body: body !== undefined ? JSON.stringify(body) : undefined,
    });
  } catch (err) {
    throw new Error('Cannot reach backend API. Check Docker containers, CORS, and VITE_API_URL.');
  }

  const text = await res.text();
  let json = {};
  try { json = text ? JSON.parse(text) : {}; } catch { json = { raw: text }; }
  if (!res.ok) throw new Error(json.raw || text || `HTTP ${res.status}`);
  return json;
}

function Card({ title, children, action }) {
  return (
    <div className="card">
      <div className="card-header">
        <h3>{title}</h3>
        {action}
      </div>
      {children}
    </div>
  );
}

function Metric({ label, value }) {
  return (
    <div className="metric">
      <div className="metric-label">{label}</div>
      <div className="metric-value">{value}</div>
    </div>
  );
}

function StepCard({ title, description, done, actionLabel, onAction }) {
  return (
    <div className={done ? 'step-card done' : 'step-card'}>
      <div className="step-card-top">
        <div>
          <div className="step-title">{title}</div>
          <div className="muted">{description}</div>
        </div>
        <span className={done ? 'status-badge done' : 'status-badge pending'}>
          {done ? 'Done' : 'Pending'}
        </span>
      </div>
      {onAction && (
        <div className="step-actions">
          <button className="ghost" type="button" onClick={onAction}>{actionLabel}</button>
        </div>
      )}
    </div>
  );
}

function Register({ onAuthed, onSwitch }) {
  const [plans, setPlans] = useState([]);
  const [form, setForm] = useState({
    first_name: '',
    last_name: '',
    email: '',
    password: '',
    company_name: '',
    country: '',
    company_size: '1-10',
    plan_code: 'starter',
  });
  const [error, setError] = useState('');

  useEffect(() => {
    api('/api/plans').then((d) => setPlans(d.plans || [])).catch(() => { });
  }, []);

  const submit = async (e) => {
    e.preventDefault();
    setError('');
    try {
      const res = await api('/api/auth/register', { method: 'POST', body: form });
      localStorage.setItem('token', res.token);
      onAuthed(res.token);
    } catch (err) {
      setError(err.message);
    }
  };

  return (
    <form className="auth-card" onSubmit={submit}>
      <div className="auth-card-heading">
        <h1>Create your workspace</h1>
        <p>Start with registration, then follow the guided setup to connect Jira and train your first tenant model.</p>
      </div>
      <div className="grid two">
        <input placeholder="First name" value={form.first_name} onChange={(e) => setForm({ ...form, first_name: e.target.value })} required />
        <input placeholder="Last name" value={form.last_name} onChange={(e) => setForm({ ...form, last_name: e.target.value })} required />
      </div>
      <input placeholder="Work email" type="email" value={form.email} onChange={(e) => setForm({ ...form, email: e.target.value })} required />
      <input placeholder="Password" type="password" value={form.password} onChange={(e) => setForm({ ...form, password: e.target.value })} required />
      <input placeholder="Company name" value={form.company_name} onChange={(e) => setForm({ ...form, company_name: e.target.value })} required />
      <div className="grid two">
        <input placeholder="Country" value={form.country} onChange={(e) => setForm({ ...form, country: e.target.value })} />
        <select value={form.company_size} onChange={(e) => setForm({ ...form, company_size: e.target.value })}>
          <option>1-10</option>
          <option>11-50</option>
          <option>51-200</option>
          <option>200+</option>
        </select>
      </div>
      <select value={form.plan_code} onChange={(e) => setForm({ ...form, plan_code: e.target.value })}>
        {plans.map((p) => (
          <option key={p.code} value={p.code}>{p.name} — {p.monthly_predictions} predictions/month</option>
        ))}
      </select>
      {error && <div className="error">{error}</div>}
      <div className="auth-actions">
        <button type="submit">Create company</button>
        <button className="ghost" type="button" onClick={onSwitch}>Already have an account? Log in</button>
      </div>
    </form>
  );
}

function Login({ onAuthed, onSwitch }) {
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [error, setError] = useState('');

  const submit = async (e) => {
    e.preventDefault();
    setError('');
    try {
      const res = await api('/api/auth/login', { method: 'POST', body: { email, password } });
      localStorage.setItem('token', res.token);
      onAuthed(res.token);
    } catch (err) {
      setError(err.message);
    }
  };

  return (
    <form className="auth-card" onSubmit={submit}>
      <div className="auth-card-heading">
        <h1>Log in</h1>
        <p>Continue with your saved Jira connection, synced issues, analytics, and private model state.</p>
      </div>
      <input placeholder="Email" type="email" value={email} onChange={(e) => setEmail(e.target.value)} required />
      <input placeholder="Password" type="password" value={password} onChange={(e) => setPassword(e.target.value)} required />
      {error && <div className="error">{error}</div>}
      <div className="auth-actions">
        <button type="submit">Login</button>
        <button className="ghost" type="button" onClick={onSwitch}>Need a new company account?</button>
      </div>
    </form>
  );
}

function AuthHome({ screen, setScreen, onAuthed }) {
  return (
    <div className="auth-shell">
      <div className="auth-layout">
        <section className="auth-hero card">
          <div className="eyebrow">Diploma-ready ML platform</div>
          <h1>Estimate Jira work with a private model trained on your team’s own issue history.</h1>
          <p className="muted">
            React frontend, C++/Crow backend, PostgreSQL storage, XGBoost inference, Python retraining,
            Jira sync, webhook updates, analytics, and usage tracking.
          </p>
          <div className="hero-points">
            <div className="hero-point">Use Jira as the source of truth</div>
            <div className="hero-point">Show effort estimates and confidence</div>
            <div className="hero-point">Retrain per organization, isolated from others</div>
          </div>
        </section>

        <section className="auth-panel">
          <div className="auth-mode-switch">
            <button
              className={screen === 'login' ? 'mode-btn active' : 'mode-btn'}
              type="button"
              onClick={() => setScreen('login')}
            >
              Login
            </button>
            <button
              className={screen === 'register' ? 'mode-btn active' : 'mode-btn'}
              type="button"
              onClick={() => setScreen('register')}
            >
              Register
            </button>
          </div>

          {screen === 'register'
            ? <Register onAuthed={onAuthed} onSwitch={() => setScreen('login')} />
            : <Login onAuthed={onAuthed} onSwitch={() => setScreen('register')} />}
        </section>
      </div>
    </div>
  );
}

function Dashboard({ token, me }) {
  const [dashboard, setDashboard] = useState(null);
  const [analytics, setAnalytics] = useState(null);
  const [billing, setBilling] = useState(null);
  const [model, setModel] = useState(null);
  const [issues, setIssues] = useState([]);
  const [jiraForm, setJiraForm] = useState({ base_url: '', user_email: '', api_token: '', project_keys: '' });
  const [actualHoursInput, setActualHoursInput] = useState({});
  const [message, setMessage] = useState('');
  const [activeTab, setActiveTab] = useState('dashboard');
  const [editingJira, setEditingJira] = useState(true);
  const [onboardingComplete, setOnboardingComplete] = useState(true);

  const webhookUrl = buildWebhookUrl(me.organization_slug);
  const jiraStorageKey = `ai-task-estimator:jira:${me.organization_slug}`;
  const onboardingStorageKey = `ai-task-estimator:onboarding:${me.organization_slug}`;

  useEffect(() => {
    const savedConnection = localStorage.getItem(jiraStorageKey);
    if (savedConnection) {
      try {
        setJiraForm((prev) => ({ ...prev, ...JSON.parse(savedConnection) }));
        setEditingJira(false);
      } catch {
        setEditingJira(true);
      }
    } else {
      setEditingJira(true);
    }

    const done = localStorage.getItem(onboardingStorageKey) === '1';
    setOnboardingComplete(done);
    setActiveTab(done ? 'dashboard' : 'getting-started');
  }, [jiraStorageKey, onboardingStorageKey]);

  const load = async () => {
    const [d, a, b, m, i] = await Promise.all([
      api('/api/dashboard', { token }),
      api('/api/analytics/overview', { token }),
      api('/api/billing/usage', { token }),
      api('/api/model/status', { token }),
      api('/api/issues', { token }),
    ]);
    setDashboard(d);
    setAnalytics(a);
    setBilling(b);
    setModel(m);
    setIssues(i.issues || []);
  };

  useEffect(() => {
    load().catch((e) => setMessage(e.message));
  }, []);

  const saveJira = async () => {
    const cleaned = {
      base_url: jiraForm.base_url.trim(),
      user_email: jiraForm.user_email.trim(),
      api_token: jiraForm.api_token.trim(),
      project_keys: parseCsv(jiraForm.project_keys).join(','),
    };

    await api('/api/jira/connect', { method: 'POST', token, body: cleaned });
    localStorage.setItem(jiraStorageKey, JSON.stringify(cleaned));
    setJiraForm(cleaned);
    setEditingJira(false);
    setMessage('Jira connection saved.');
  };

  const syncJira = async () => {
    await api('/api/jira/sync', { method: 'POST', token });
    setMessage('Jira sync completed.');
    await load();
  };

  const saveActual = async (id) => {
    const actual_hours = parseFloat(actualHoursInput[id]);
    if (Number.isNaN(actual_hours) || actual_hours <= 0) return;
    await api(`/api/issues/${id}/actual-hours`, { method: 'POST', token, body: { actual_hours } });
    setMessage('Actual hours saved.');
    await load();
  };

  const retrain = async () => {
    await api('/api/model/retrain', { method: 'POST', token });
    setMessage('Retraining completed.');
    await load();
  };

  const completeOnboarding = () => {
    localStorage.setItem(onboardingStorageKey, '1');
    setOnboardingComplete(true);
    setActiveTab('dashboard');
    setMessage('Setup guide completed. You can reopen it anytime from the sidebar.');
  };

  const connectedProjects = useMemo(() => {
    return uniq([
      ...parseCsv(jiraForm.project_keys),
      ...issues.map((issue) => issue.jira_project_key),
    ]);
  }, [jiraForm.project_keys, issues]);

  const sortedIssues = useMemo(() => {
    return [...issues].sort((a, b) => {
      const dateDiff = new Date(b.updated_at || 0).getTime() - new Date(a.updated_at || 0).getTime();
      if (dateDiff !== 0) return dateDiff;
      return (b.id || 0) - (a.id || 0);
    });
  }, [issues]);

  const issueRows = useMemo(() => sortedIssues.map((issue) => (
    <tr key={issue.id}>
      <td>{issue.jira_key}</td>
      <td>{issue.summary}</td>
      <td>{issue.status}</td>
      <td>{issue.priority_name}</td>
      <td>{Number(issue.estimated_hours || 0).toFixed(2)}h</td>
      <td>{issue.confidence ? `${Math.round(issue.confidence * 100)}%` : '-'}</td>
      <td>
        {issue.actual_hours >= 0 ? (
          <span>{Number(issue.actual_hours).toFixed(2)}h</span>
        ) : (
          <div className="inline-edit">
            <input type="number" step="0.1" placeholder="actual" onChange={(e) => setActualHoursInput({ ...actualHoursInput, [issue.id]: e.target.value })} />
            <button onClick={() => saveActual(issue.id)}>Save</button>
          </div>
        )}
      </td>
    </tr>
  )), [sortedIssues, actualHoursInput]);

  const setupSteps = useMemo(() => {
    const jiraSaved = Boolean(jiraForm.base_url && jiraForm.user_email && jiraForm.project_keys);
    const hasSyncedIssues = issues.length > 0;
    const hasActuals = issues.some((issue) => Number(issue.actual_hours) >= 0);
    const retrainEligible = Boolean(model?.retrain_eligible);
    const privateModelActive = model?.model_type === 'private';

    return [
      {
        title: '1. Connect Jira',
        description: 'Save your Jira URL, Atlassian email, API token, and the Jira project keys you want to sync.',
        done: jiraSaved,
        actionLabel: 'Open Jira connection',
        onAction: () => setActiveTab('jira'),
      },
      {
        title: '2. Sync your first issues',
        description: 'Pull issues from Jira so the system can run predictions and show them in the overview.',
        done: hasSyncedIssues,
        actionLabel: 'Open synced issues',
        onAction: () => setActiveTab('issues'),
      },
      {
        title: '3. Add actual hours',
        description: 'For completed issues, add real execution time so your organization starts collecting training examples.',
        done: hasActuals,
        actionLabel: 'Review issues',
        onAction: () => setActiveTab('issues'),
      },
      {
        title: '4. Check retraining eligibility',
        description: 'Once enough resolved issues have actual hours, the platform can build a tenant-private model.',
        done: retrainEligible,
        actionLabel: 'Open model status',
        onAction: () => setActiveTab('model'),
      },
      {
        title: '5. Train and activate your private model',
        description: 'Run retraining to switch from the foundation model to a model adapted to your own Jira history.',
        done: privateModelActive,
        actionLabel: 'Go to model page',
        onAction: () => setActiveTab('model'),
      },
    ];
  }, [jiraForm, issues, model]);

  const onboardingProgress = Math.round((setupSteps.filter((step) => step.done).length / setupSteps.length) * 100);

  const removeConnectedProject = (projectKey) => {
    const nextKeys = parseCsv(jiraForm.project_keys).filter((key) => key !== projectKey).join(',');
    setJiraForm((prev) => ({ ...prev, project_keys: nextKeys }));
    setEditingJira(true);
  };

  const detectedProjectsFromIssues = uniq(issues.map((issue) => issue.jira_project_key).filter(Boolean));

  const restoreDetectedProjects = () => {
    setJiraForm((prev) => ({ ...prev, project_keys: detectedProjectsFromIssues.join(',') }));
    setEditingJira(true);
  };

  if (!dashboard || !analytics || !billing || !model) return <div className="loading">Loading…</div>;

  return (
    <div className="app-shell">
      <aside className="sidebar">
        <div>
          <div className="brand">AI Task Estimator</div>
          <div className="muted">{me.organization_name}</div>
          <div className="muted small">{me.plan_name}</div>
        </div>
        <nav>
          {[
            ['getting-started', 'Getting started'],
            ['dashboard', 'Dashboard'],
            ['jira', 'Jira'],
            ['issues', 'Issues'],
            ['analytics', 'Analytics'],
            ['model', 'Model'],
            ['billing', 'Billing'],
          ].map(([tab, label]) => (
            <button key={tab} className={activeTab === tab ? 'nav-btn active' : 'nav-btn'} onClick={() => setActiveTab(tab)}>{label}</button>
          ))}
        </nav>
        <button className="ghost" onClick={() => { localStorage.removeItem('token'); window.location.reload(); }}>Logout</button>
      </aside>
      <main className="content">
        <header className="topbar">
          <div>
            <h1>Welcome, {me.first_name}</h1>
            <p>Jira-centered ML estimation platform with tenant-private retraining.</p>
          </div>
          <div className="token-pill">Tokens left: {dashboard.tokens_left}</div>
        </header>

        {!onboardingComplete && (
          <div className="notice onboarding-banner">
            <div>
              <strong>Finish setup</strong>
              <div className="muted">Complete the onboarding guide to connect Jira, sync issues, and train your first private model.</div>
            </div>
            <button className="ghost" onClick={() => setActiveTab('getting-started')}>Open guide</button>
          </div>
        )}

        {message && <div className="notice">{message}</div>}

        {activeTab === 'getting-started' && (
          <div className="grid two-col onboarding-grid">
            <Card
              title="Workspace setup guide"
              action={!onboardingComplete ? <button onClick={completeOnboarding}>Finish guide</button> : <button className="ghost" onClick={() => setOnboardingComplete(false)}>Reopen guide</button>}
            >
              <div className="progress-block">
                <div className="progress-line">
                  <div className="progress-fill" style={{ width: `${onboardingProgress}%` }} />
                </div>
                <div className="muted small">{onboardingProgress}% completed</div>
              </div>
              <div className="step-list">
                {setupSteps.map((step) => (
                  <StepCard key={step.title} {...step} />
                ))}
              </div>
            </Card>

            <Card title="What happens after setup">
              <div className="simple-list">
                <div className="list-row"><strong>Sync scope</strong><span>Jira project keys define what gets imported.</span></div>
                <div className="list-row"><strong>Predictions</strong><span>Each synced issue gets an ML estimate and confidence score.</span></div>
                <div className="list-row"><strong>Feedback loop</strong><span>Actual hours turn completed issues into training data.</span></div>
                <div className="list-row"><strong>Private model</strong><span>Retraining stays isolated to your organization only.</span></div>
              </div>
            </Card>
          </div>
        )}

        {activeTab === 'dashboard' && (
          <>
            <div className="metrics-grid">
              <Metric label="Total synced issues" value={dashboard.total_issues} />
              <Metric label="Resolved with actuals" value={dashboard.resolved_with_actuals} />
              <Metric label="Avg estimated" value={`${Number(dashboard.avg_estimated_hours).toFixed(2)}h`} />
              <Metric label="Avg actual" value={`${Number(dashboard.avg_actual_hours).toFixed(2)}h`} />
              <Metric label="Active model" value={dashboard.active_model_type} />
              <Metric label="Tokens left" value={dashboard.tokens_left} />
            </div>
            <Card title="How this version matches the diploma requirements">
              <ul>
                <li>Uses Jira as the issue source instead of a custom board clone.</li>
                <li>Runs prediction in C++ with the XGBoost C API.</li>
                <li>Retrains per organization from organization-local issue history only.</li>
                <li>Keeps analytics and token usage per tenant.</li>
              </ul>
            </Card>
          </>
        )}

        {activeTab === 'jira' && (
          <div className="grid two-col">
            <Card
              title="Jira connection"
              action={<button className="ghost" onClick={() => setEditingJira((v) => !v)}>{editingJira ? 'Hide editor' : 'Edit connection'}</button>}
            >
              {!editingJira ? (
                <div className="simple-list">
                  <div className="list-row"><strong>Base URL</strong><span>{jiraForm.base_url || '-'}</span></div>
                  <div className="list-row"><strong>User</strong><span>{jiraForm.user_email || '-'}</span></div>
                  <div className="list-row"><strong>Connected boards/projects</strong><span>{connectedProjects.join(', ') || 'None yet'}</span></div>
                  <div className="actions">
                    <button onClick={syncJira}>Sync now</button>
                    <button className="ghost" onClick={() => setEditingJira(true)}>Edit</button>
                  </div>
                </div>
              ) : (
                <div className="form-stack">
                  <input placeholder="https://your-company.atlassian.net" value={jiraForm.base_url} onChange={(e) => setJiraForm({ ...jiraForm, base_url: e.target.value })} />
                  <input placeholder="Atlassian email" value={jiraForm.user_email} onChange={(e) => setJiraForm({ ...jiraForm, user_email: e.target.value })} />
                  <input placeholder="API token" value={jiraForm.api_token} onChange={(e) => setJiraForm({ ...jiraForm, api_token: e.target.value })} />
                  <input placeholder="Project keys / boards, e.g. ABC,PLATFORM" value={jiraForm.project_keys} onChange={(e) => setJiraForm({ ...jiraForm, project_keys: e.target.value })} />
                  <div className="actions">
                    <button onClick={saveJira}>Save connection</button>
                    <button className="ghost" onClick={syncJira}>Sync now</button>
                  </div>
                </div>
              )}
            </Card>

            <Card title="Connected Jira boards / projects">
              {connectedProjects.length > 0 ? (
                <div className="chip-grid">
                  {connectedProjects.map((projectKey) => (
                    <div key={projectKey} className="chip">
                      <span>{projectKey}</span>
                      <button className="chip-remove" onClick={() => removeConnectedProject(projectKey)} title={`Remove ${projectKey} from editable project list`}>×</button>
                    </div>
                  ))}
                </div>
              ) : (
                <div className="empty-state">
                  No connected boards detected yet. Save a Jira connection and sync to populate this section.
                </div>
              )}

              {detectedProjectsFromIssues.length > 0 && parseCsv(jiraForm.project_keys).length === 0 && (
                <div className="actions top-space">
                  <button className="ghost" onClick={restoreDetectedProjects}>Use detected projects in editor</button>
                </div>
              )}

              <p className="muted small top-space">
                The current integration uses Jira project keys as the sync scope. Editing this list changes which Jira boards/projects are included the next time you sync.
              </p>
            </Card>

            <Card title="Webhook setup">
              <p>In Jira, point the webhook to:</p>
              <code>{webhookUrl}</code>
              <p className="muted">Recommended events: issue created, issue updated, issue deleted.</p>
              <p className="muted small">
                This URL adapts automatically to your current public app address. For ngrok testing, open the app via your ngrok URL and this webhook will update automatically.
              </p>
            </Card>
          </div>
        )}

        {activeTab === 'issues' && (
          <Card title="Synced Jira issues" action={<button onClick={syncJira}>Refresh from Jira</button>}>
            <div className="table-wrap">
              <table>
                <thead>
                  <tr>
                    <th>Key</th><th>Summary</th><th>Status</th><th>Priority</th><th>ML estimate</th><th>Confidence</th><th>Actual hours</th>
                  </tr>
                </thead>
                <tbody>{issueRows}</tbody>
              </table>
            </div>
          </Card>
        )}

        {activeTab === 'analytics' && (
          <div className="grid two-col">
            <Card title="Accuracy metrics">
              <div className="metrics-grid compact">
                <Metric label="Samples with actuals" value={analytics.samples_with_actuals} />
                <Metric label="MAE" value={Number(analytics.mae).toFixed(3)} />
                <Metric label="RMSE" value={Number(analytics.rmse).toFixed(3)} />
                <Metric label="MAPE" value={`${(Number(analytics.mape) * 100).toFixed(2)}%`} />
              </div>
            </Card>
            <Card title="By priority">
              <div className="simple-list">
                {(analytics.by_priority || []).map((row, idx) => (
                  <div key={idx} className="list-row">
                    <strong>{row.priority}</strong>
                    <span>{row.count} issues</span>
                    <span>avg est {Number(row.avg_estimated).toFixed(2)}h</span>
                    <span>avg actual {Number(row.avg_actual).toFixed(2)}h</span>
                  </div>
                ))}
              </div>
            </Card>
          </div>
        )}

        {activeTab === 'model' && (
          <div className="grid two-col">
            <Card title="Model status" action={<button onClick={retrain} disabled={!model.retrain_eligible}>Retrain private model</button>}>
              <div className="simple-list">
                <div className="list-row"><strong>Current model</strong><span>{model.model_type}</span></div>
                <div className="list-row"><strong>Version</strong><span>{model.version_name}</span></div>
                <div className="list-row"><strong>Training samples</strong><span>{model.actual_samples}</span></div>
                <div className="list-row"><strong>Retrain eligible</strong><span>{String(model.retrain_eligible)}</span></div>
                <div className="list-row"><strong>MAE</strong><span>{Number(model.mae || 0).toFixed(4)}</span></div>
                <div className="list-row"><strong>RMSE</strong><span>{Number(model.rmse || 0).toFixed(4)}</span></div>
              </div>
            </Card>
            <Card title="Private model logic">
              <ul>
                <li>Foundation model is used for cold start.</li>
                <li>Private organization model is trained only from your organization’s resolved issues.</li>
                <li>Inference falls back to the foundation model if a private model does not yet exist.</li>
              </ul>
            </Card>
          </div>
        )}

        {activeTab === 'billing' && (
          <Card title="Tariff and token usage">
            <div className="metrics-grid">
              <Metric label="Plan" value={billing.plan_name} />
              <Metric label="Users" value={billing.users_count} />
              <Metric label="Prediction limit" value={billing.monthly_predictions_limit} />
              <Metric label="Token limit" value={billing.monthly_tokens_limit} />
              <Metric label="Tokens used" value={billing.tokens_used} />
              <Metric label="Retrains" value={billing.retraining_calls} />
            </div>
          </Card>
        )}
      </main>
    </div>
  );
}

export default function App() {
  const [token, setToken] = useState(localStorage.getItem('token'));
  const [me, setMe] = useState(null);
  const [screen, setScreen] = useState('login');

  useEffect(() => {
    if (!token) return;
    api('/api/me', { token }).then(setMe).catch(() => {
      localStorage.removeItem('token');
      setToken('');
      setMe(null);
    });
  }, [token]);

  if (!token) {
    return <AuthHome screen={screen} setScreen={setScreen} onAuthed={setToken} />;
  }

  if (!me) return <div className="loading">Loading session…</div>;
  return <Dashboard token={token} me={me} />;
}
