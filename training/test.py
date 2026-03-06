import numpy as np
import xgboost as xgb
import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_absolute_error, mean_squared_error
from sklearn.feature_extraction.text import TfidfVectorizer, CountVectorizer

def eval_model(X, y):
    Xtr, Xva, ytr, yva = train_test_split(X, y, test_size=0.2, random_state=42)
    m = xgb.XGBRegressor(objective="reg:squarederror", n_estimators=300, learning_rate=0.05, max_depth=5)
    m.fit(Xtr, ytr)
    p = m.predict(Xva)
    mae = mean_absolute_error(yva, p)
    rmse = np.sqrt(mean_squared_error(yva, p))
    return mae, rmse

data = [
  {
    "desc": "Add missing indexes for the search queries to reduce load time",
    "prio": 1,
    "role": 2,
    "hours": 10.0
  },
  {
    "desc": "Hotfix: Implement MFA (TOTP) setup for production all auth endpoints and store refresh tokens securely",
    "prio": 3,
    "role": 2,
    "hours": 17.5
  },
  {
    "desc": "Implement SSO via SAML for mobile app and add abuse monitoring",
    "prio": 1,
    "role": 2,
    "hours": 38.5
  },
  {
    "desc": "Setup AWS S3 bucket for worker queue with encrypted storage",
    "prio": 1,
    "role": 0,
    "hours": 6.5
  },
  {
    "desc": "Add password reset flow for mobile app with strict redirect URI validation",
    "prio": 1,
    "role": 2,
    "hours": 10.5
  },
  {
    "desc": "Add OpenTelemetry spans for webhook throughput with meaningful labels (for EU users)",
    "prio": 1,
    "role": 0,
    "hours": 3.0
  },
  {
    "desc": "Optimize database queries for the reporting dashboard to reduce deadlocks (and add monitoring)",
    "prio": 2,
    "role": 2,
    "hours": 14.0
  },
  {
    "desc": "Fix typo in the data retention policy and update links",
    "prio": 0,
    "role": 0,
    "hours": 1.0
  },
  {
    "desc": "Implement CRUD endpoints for audit logs with request tracing (with detailed logging)",
    "prio": 1,
    "role": 2,
    "hours": 17.0
  },
  {
    "desc": "Update docker-compose with Redis cache with automated rollback (in staging)",
    "prio": 1,
    "role": 1,
    "hours": 8.5
  },
  {
    "desc": "Create dashboards for payment retries and define thresholds (for high-traffic events)",
    "prio": 2,
    "role": 1,
    "hours": 8.0
  },
  {
    "desc": "Refactor legacy JavaScript code to TypeScript in the checkout flow to improve type safety (phase 1)",
    "prio": 1,
    "role": 2,
    "hours": 27.0
  },
  {
    "desc": "Write unit tests for the OAuth callback handler including error paths (with CI parallelization)",
    "prio": 1,
    "role": 1,
    "hours": 9.0
  },
  {
    "desc": "Add security headers to checkout and cover edge cases (for desktop)",
    "prio": 2,
    "role": 1,
    "hours": 12.0
  },
  {
    "desc": "Implement idempotency for payments with retries/backoff (for US users)",
    "prio": 2,
    "role": 2,
    "hours": 18.5
  },
  {
    "desc": "Improve responsive layout for cookie banner (Tailwind CSS) to meet WCAG AA (for accessibility compliance)",
    "prio": 1,
    "role": 0,
    "hours": 3.0
  },
  {
    "desc": "Add structured logging to database connections and reduce cardinality (in production)",
    "prio": 2,
    "role": 1,
    "hours": 12.5
  },
  {
    "desc": "Document runbook for the incident response playbook and include ownership info",
    "prio": 0,
    "role": 0,
    "hours": 2.0
  },
  {
    "desc": "Tune SQL execution plan for the catalog page to remove full table scans (for high-traffic events)",
    "prio": 2,
    "role": 2,
    "hours": 24.0
  },
  {
    "desc": "Implement Google OAuth 2.0 flow for web app with PKCE support (phase 1)",
    "prio": 1,
    "role": 2,
    "hours": 20.0
  },
  {
    "desc": "Stabilize flaky tests for the checkout errors with deterministic time (in staging)",
    "prio": 2,
    "role": 1,
    "hours": 9.5
  },
  {
    "desc": "Add rate limiting to sessions with RBAC checks (in production)",
    "prio": 2,
    "role": 2,
    "hours": 11.0
  },
  {
    "desc": "Configure CI pipeline for backend service with retention policy (phase 2)",
    "prio": 1,
    "role": 1,
    "hours": 13.5
  },
  {
    "desc": "Instrument tracing for search response time and track p95/p99 (for multi-tenant setup)",
    "prio": 1,
    "role": 1,
    "hours": 10.0
  },
  {
    "desc": "Upgrade dependencies for the notification service to reduce build time",
    "prio": 1,
    "role": 2,
    "hours": 33.0
  },
  {
    "desc": "Implement connection pooling for the user profile lookup to improve cache hit rate",
    "prio": 1,
    "role": 2,
    "hours": 17.0
  },
  {
    "desc": "Fix Safari rendering issue in search bar (SCSS) when cart has many items",
    "prio": 1,
    "role": 0,
    "hours": 2.0
  },
  {
    "desc": "Add webhook receiver for orders with retries/backoff (with backward compatibility)",
    "prio": 2,
    "role": 2,
    "hours": 16.0
  },
  {
    "desc": "Create end-to-end tests for the mobile checkout and improve assertion messages (for desktop)",
    "prio": 1,
    "role": 1,
    "hours": 15.5
  },
  {
    "desc": "Harden IAM permissions for static assets with least privilege (for SOC2 audit)",
    "prio": 2,
    "role": 2,
    "hours": 11.5
  },
  {
    "desc": "Add regression tests for the image upload endpoint covering concurrency issues (phase 2)",
    "prio": 1,
    "role": 1,
    "hours": 10.0
  },
  {
    "desc": "Reduce log noise for login failures and add runbook links (in production)",
    "prio": 3,
    "role": 1,
    "hours": 6.0
  },
  {
    "desc": "Rewrite N+1 queries in the order history to reduce timeouts (for EU users)",
    "prio": 2,
    "role": 2,
    "hours": 19.0
  },
  {
    "desc": "Fix CSS alignment on header (Bootstrap) on small screens",
    "prio": 1,
    "role": 0,
    "hours": 1.5
  },
  {
    "desc": "Implement audit trail for admin roles with structured logs (phase 1)",
    "prio": 1,
    "role": 2,
    "hours": 18.0
  },
  {
    "desc": "Add API examples to the API documentation and fix formatting",
    "prio": 0,
    "role": 0,
    "hours": 3.0
  },
  {
    "desc": "Set up alerting rules for S3 upload failures and add runbook links (in production)",
    "prio": 3,
    "role": 1,
    "hours": 8.0
  },
  {
    "desc": "Implement background job processing for notifications with correlation IDs (for multi-tenant setup)",
    "prio": 1,
    "role": 2,
    "hours": 21.0
  },
  {
    "desc": "Modularize CSS and remove dead code in the legacy UI bundle to reduce cognitive load",
    "prio": 1,
    "role": 2,
    "hours": 28.0
  },
  {
    "desc": "Migrate core ML training pipeline from Airflow to Kubeflow to better handle GPU scheduling",
    "prio": 2,
    "role": 2,
    "hours": 35.0
  },
  {
    "desc": "Fix memory leak in iOS app when swiping quickly through the high-res image gallery",
    "prio": 2,
    "role": 2,
    "hours": 12.5
  },
  {
    "desc": "Update HIPAA compliance documentation for the new patient portal architecture",
    "prio": 0,
    "role": 0,
    "hours": 6.0
  },
  {
    "desc": "Hotfix: Rollback broken firmware OTA update causing boot loops on v2 smart thermostats",
    "prio": 3,
    "role": 2,
    "hours": 8.0
  },
  {
    "desc": "Optimize occlusion culling in the downtown map to hit steady 60fps on mid-tier consoles",
    "prio": 1,
    "role": 2,
    "hours": 24.0
  },
  {
    "desc": "Implement HL7 FHIR parser for ingesting electronic health records from legacy hospital systems",
    "prio": 2,
    "role": 2,
    "hours": 32.0
  },
  {
    "desc": "Add Playwright end-to-end tests for the new multi-step onboarding wizard",
    "prio": 1,
    "role": 1,
    "hours": 14.5
  },
  {
    "desc": "Refactor Terraform state files to separate staging and production VPC configurations",
    "prio": 2,
    "role": 1,
    "hours": 16.0
  },
  {
    "desc": "Implement Jetpack Compose navigation in the Android app settings screen",
    "prio": 1,
    "role": 2,
    "hours": 18.0
  },
  {
    "desc": "Fix typo in the 'Account Suspended' email template",
    "prio": 0,
    "role": 0,
    "hours": 0.5
  },
  {
    "desc": "Mask PII and credit card numbers in the Kibana logging stack",
    "prio": 3,
    "role": 1,
    "hours": 5.5
  },
  {
    "desc": "Update Pandas dataframe operations in the nightly ETL job to prevent OOM errors",
    "prio": 2,
    "role": 2,
    "hours": 11.0
  },
  {
    "desc": "Write a runbook for handling Kafka partition rebalancing incidents",
    "prio": 1,
    "role": 1,
    "hours": 4.0
  },
  {
    "desc": "Implement ISO 8583 message parsing for the new international payment gateway",
    "prio": 2,
    "role": 2,
    "hours": 40.0
  },
  {
    "desc": "Adjust dark mode CSS variables to fix low contrast text in the dashboard footer",
    "prio": 1,
    "role": 0,
    "hours": 2.5
  },
  {
    "desc": "Set up a Dead Letter Queue (DLQ) for the SMS notification service and add Datadog alerts",
    "prio": 2,
    "role": 1,
    "hours": 9.0
  },
  {
    "desc": "Refactor Unity inventory system to use ScriptableObjects instead of singletons",
    "prio": 1,
    "role": 2,
    "hours": 22.0
  },
  {
    "desc": "Retrain XGBoost churn prediction model to account for recent seasonal data drift",
    "prio": 1,
    "role": 2,
    "hours": 28.5
  },
  {
    "desc": "Add basic Swagger annotations to the legacy user management endpoints",
    "prio": 0,
    "role": 0,
    "hours": 7.5
  },
  {
    "desc": "Hotfix: Patch critical vulnerability in the log4j dependency across all microservices",
    "prio": 3,
    "role": 1,
    "hours": 14.0
  },
  {
    "desc": "Implement offline caching for the mobile field-worker app using SQLite",
    "prio": 2,
    "role": 2,
    "hours": 30.0
  },
  {
    "desc": "Fix race condition in the C++ matchmaking server causing players to get stuck in the lobby",
    "prio": 3,
    "role": 2,
    "hours": 18.5
  },
  {
    "desc": "Upgrade React Native version from 0.68 to 0.72 and resolve breaking changes",
    "prio": 1,
    "role": 2,
    "hours": 36.0
  },
  {
    "desc": "Create a Grafana dashboard to monitor Kubernetes pod CPU/Memory limits and evictions",
    "prio": 1,
    "role": 1,
    "hours": 6.5
  },
  {
    "desc": "Implement hardware watchdog timer in the RTOS to reboot frozen IoT sensors",
    "prio": 2,
    "role": 2,
    "hours": 15.0
  },
  {
    "desc": "Add strictly typed gRPC stubs for the new billing service and distribute to client teams",
    "prio": 1,
    "role": 1,
    "hours": 8.0
  },
  {
    "desc": "Fix padding issues on the generic error page for tablet viewports",
    "prio": 0,
    "role": 0,
    "hours": 1.5
  },
  {
    "desc": "Implement real-time collaboration using WebSockets for the document editor feature",
    "prio": 2,
    "role": 2,
    "hours": 38.0
  },
  {
    "desc": "Automate rotation of AWS RDS database credentials using AWS Secrets Manager",
    "prio": 2,
    "role": 1,
    "hours": 12.0
  },
  {
    "desc": "Tweak physics material friction on the player character to prevent sliding on slopes",
    "prio": 1,
    "role": 0,
    "hours": 3.0
  },
  {
    "desc": "Write a Python script to backfill missing historical exchange rate data from the API",
    "prio": 1,
    "role": 1,
    "hours": 6.5
  },
  {
    "desc": "Add exponential backoff to MQTT client reconnections to prevent server DDOS after network partitions",
    "prio": 2,
    "role": 2,
    "hours": 10.0
  },
  {
    "desc": "Hotfix: Fix incorrect timezone offset causing daily reports to generate twice",
    "prio": 3,
    "role": 2,
    "hours": 4.5
  },
  {
    "desc": "Create comprehensive UX copy guidelines for empty states across the dashboard",
    "prio": 0,
    "role": 0,
    "hours": 5.0
  },
  {
    "desc": "Setup CI/CD pipeline using GitHub Actions for the Rust webassembly module",
    "prio": 1,
    "role": 1,
    "hours": 11.0
  },
  {
    "desc": "Implement optical character recognition (OCR) pipeline for extracting data from uploaded receipts",
    "prio": 2,
    "role": 2,
    "hours": 45.0
  },
  {
    "desc": "Fix accessibility issue: Add aria-labels to all icon-only buttons in the navigation bar",
    "prio": 1,
    "role": 0,
    "hours": 2.0
  },
  {
    "desc": "Refactor monolithic Node.js backend into user and inventory microservices (Phase 1)",
    "prio": 2,
    "role": 2,
    "hours": 40.0
  },
  {
    "desc": "Debug and fix irregular voltage spikes detected in the battery management system firmware",
    "prio": 3,
    "role": 2,
    "hours": 25.0
  },
  {
    "desc": "Migrate image hosting from self-managed servers to AWS CloudFront CDN",
    "prio": 1,
    "role": 1,
    "hours": 16.5
  },
  {
    "desc": "Add 'Forgot Password' UI flow for the iOS native app and wire up backend calls",
    "prio": 1,
    "role": 2,
    "hours": 14.0
  },
  {
    "desc": "Update the README.md to include local Docker development setup instructions",
    "prio": 0,
    "role": 0,
    "hours": 2.5
  },
  {
    "desc": "Implement A/B testing framework hooks in the Next.js frontend application",
    "prio": 1,
    "role": 2,
    "hours": 20.0
  },
  {
    "desc": "Fix UI glitch where dropdown menus render behind the video player component",
    "prio": 1,
    "role": 0,
    "hours": 3.5
  },
  {
    "desc": "Hotfix: Prevent duplicate ACH transfers when the user double-clicks the submit button",
    "prio": 3,
    "role": 2,
    "hours": 6.0
  },
  {
    "desc": "Implement memory profiling for the Spark streaming job to reduce executor crashes",
    "prio": 2,
    "role": 1,
    "hours": 18.0
  },
  {
    "desc": "Add localization support (i18n) for Spanish and French in the React web app",
    "prio": 1,
    "role": 2,
    "hours": 28.0
  },
  {
    "desc": "Optimize 3D mesh LODs for background props to reduce draw calls in VR environment",
    "prio": 1,
    "role": 0,
    "hours": 15.0
  },
  {
    "desc": "Implement mutual TLS (mTLS) authentication between internal Go microservices",
    "prio": 2,
    "role": 2,
    "hours": 24.5
  },
  {
    "desc": "Draft the architectural decision record (ADR) for choosing PostgreSQL over MongoDB",
    "prio": 0,
    "role": 2,
    "hours": 3.0
  },
  {
    "desc": "Set up automated dependency scanning using Dependabot and triage initial alerts",
    "prio": 1,
    "role": 1,
    "hours": 5.0
  },
  {
    "desc": "Implement fuzzy search algorithm using Elasticsearch for the e-commerce product catalog",
    "prio": 2,
    "role": 2,
    "hours": 30.0
  },
  {
    "desc": "Hotfix: Fix API gateway configuration dropping POST requests with large payloads",
    "prio": 3,
    "role": 1,
    "hours": 4.0
  },
  {
    "desc": "Write unit tests for the complex tax calculation logic in the shopping cart",
    "prio": 1,
    "role": 1,
    "hours": 12.0
  },
  {
    "desc": "Integrate third-party KYC (Know Your Customer) API for the user onboarding flow",
    "prio": 2,
    "role": 2,
    "hours": 35.0
  },
  {
    "desc": "Replace deprecated React lifecycle methods with functional hooks in the user profile component",
    "prio": 1,
    "role": 0,
    "hours": 8.0
  },
  {
    "desc": "Build a CLI tool in Golang to streamline local database migrations for new developers",
    "prio": 1,
    "role": 1,
    "hours": 14.0
  },
  {
    "desc": "Fix audio desync issue in the media player when switching video bitrates on mobile networks",
    "prio": 2,
    "role": 2,
    "hours": 20.0
  },
  {
    "desc": "Update 3rd-party open source licenses page in the mobile app settings",
    "prio": 0,
    "role": 0,
    "hours": 1.5
  },
  {
    "desc": "Implement Redis rate limiting to prevent brute force attacks on the login endpoint",
    "prio": 2,
    "role": 2,
    "hours": 11.5
  },
  {
    "desc": "Configure automated database backups to cross-region S3 buckets for disaster recovery",
    "prio": 1,
    "role": 1,
    "hours": 7.0
  },
  {
    "desc": "Create synthetic monitoring checks for critical checkout pathways in production",
    "prio": 1,
    "role": 1,
    "hours": 9.5
  },
  {
    "desc": "Hotfix: Resolve deadlock in the MySQL orders table causing transaction failures",
    "prio": 3,
    "role": 2,
    "hours": 10.0
  },
  {
    "desc": "Implement continuous collision detection for fast-moving projectiles in the physics engine",
    "prio": 2,
    "role": 2,
    "hours": 26.0
  },
  {
    "desc": "Add a feature flag system to toggle experimental UI features without deploying code",
    "prio": 1,
    "role": 2,
    "hours": 18.0
  },
  {
    "desc": "Standardize logging formats across all Python microservices to comply with ELK ingestion rules",
    "prio": 1,
    "role": 1,
    "hours": 12.0
  },
  {
    "desc": "Fix text overflow issue in the German translation of the pricing table",
    "prio": 0,
    "role": 0,
    "hours": 1.0
  },
  {
    "desc": "Build a recommendation engine using collaborative filtering for the media platform",
    "prio": 2,
    "role": 2,
    "hours": 40.0
  },
  {
    "desc": "Implement biometric login (FaceID/TouchID) fallback for the banking mobile app",
    "prio": 2,
    "role": 2,
    "hours": 22.5
  },
  {
    "desc": "Audit and remove unused IAM roles and permissions in the AWS production account",
    "prio": 1,
    "role": 1,
    "hours": 8.5
  },
  {
    "desc": "Setup Prometheus alerting for anomalous drops in successful API authentications",
    "prio": 1,
    "role": 1,
    "hours": 5.5
  },
  {
    "desc": "Hotfix: Fix null pointer exception in the Android app when handling malformed push notifications",
    "prio": 3,
    "role": 2,
    "hours": 6.5
  },
  {
    "desc": "Integrate WebGL canvas rendering for the interactive data visualization dashboard",
    "prio": 2,
    "role": 2,
    "hours": 34.0
  },
  {
    "desc": "Migrate deprecated cron jobs from legacy servers to Kubernetes CronJobs",
    "prio": 1,
    "role": 1,
    "hours": 14.0
  },
  {
    "desc": "Update UI components to use the new company design system token variables",
    "prio": 1,
    "role": 0,
    "hours": 16.0
  },
  {
    "desc": "Implement chunked file uploads for the video processing service to handle >2GB files",
    "prio": 2,
    "role": 2,
    "hours": 28.0
  },
  {
    "desc": "Write release notes for the v2.4.0 software update",
    "prio": 0,
    "role": 0,
    "hours": 3.0
  },
  {
    "desc": "Optimize GraphQL resolvers to solve the N+1 query problem on the friend list endpoint",
    "prio": 2,
    "role": 2,
    "hours": 15.5
  },
  {
    "desc": "Set up automated vulnerability scanning for Docker images in the CI registry",
    "prio": 1,
    "role": 1,
    "hours": 6.0
  },
  {
    "desc": "Implement vector database (Pinecone) for semantic search capabilities in the chatbot",
    "prio": 2,
    "role": 2,
    "hours": 32.0
  },
  {
    "desc": "Hotfix: Revert misconfigured CORS policy blocking all frontend traffic",
    "prio": 3,
    "role": 1,
    "hours": 2.0
  },
  {
    "desc": "Design and implement API rate limiting tiers (free, pro, enterprise) based on API keys",
    "prio": 2,
    "role": 2,
    "hours": 26.5
  },
  {
    "desc": "Fix bug in the C# event system where unsubscribing during an event invocation causes a crash",
    "prio": 2,
    "role": 2,
    "hours": 10.0
  },
  {
    "desc": "Add a skeleton loading state for the user dashboard to improve perceived performance",
    "prio": 1,
    "role": 0,
    "hours": 5.0
  },
  {
    "desc": "Migrate the legacy REST API test suite to use PyTest and VCR.py for mocking",
    "prio": 1,
    "role": 1,
    "hours": 18.0
  },
  {
    "desc": "Refactor the CoreBluetooth wrapper on iOS to handle sudden device disconnections gracefully",
    "prio": 2,
    "role": 2,
    "hours": 21.0
  },
  {
    "desc": "Update internal wiki with architecture diagrams for the new billing microservice",
    "prio": 0,
    "role": 0,
    "hours": 4.0
  },
  {
    "desc": "Implement server-side rendering (SSR) for the public blog to improve SEO metrics",
    "prio": 2,
    "role": 2,
    "hours": 29.0
  },
  {
    "desc": "Configure strict Content Security Policy (CSP) headers to prevent XSS attacks",
    "prio": 1,
    "role": 1,
    "hours": 7.5
  },
  {
    "desc": "Hotfix: Increase max connection pool size on production database during marketing spike",
    "prio": 3,
    "role": 1,
    "hours": 1.0
  },
  {
    "desc": "Create an automated script to purge inactive guest accounts after 90 days",
    "prio": 1,
    "role": 1,
    "hours": 8.0
  },
  {
    "desc": "Integrate Stripe webhooks to handle subscription upgrades, downgrades, and cancellations",
    "prio": 2,
    "role": 2,
    "hours": 25.0
  },
  {
    "desc": "Fix clipping issues on character armor models in the character creation screen",
    "prio": 1,
    "role": 0,
    "hours": 6.5
  },
  {
    "desc": "Implement circuit breaker pattern for external third-party API calls using Resilience4j",
    "prio": 2,
    "role": 2,
    "hours": 17.0
  },
  {
    "desc": "Add custom metric tracking for 'Time to First Meaningful Paint' in Datadog",
    "prio": 1,
    "role": 1,
    "hours": 4.5
  },
  {
    "desc": "Draft API documentation for the new GraphQL schema using GraphiQL",
    "prio": 0,
    "role": 0,
    "hours": 8.0
  },
  {
    "desc": "Implement fine-grained Role-Based Access Control (RBAC) middleware for the admin panel",
    "prio": 2,
    "role": 2,
    "hours": 23.0
  },
  {
    "desc": "Fix Safari-specific bug where the sticky header loses its position on scroll",
    "prio": 1,
    "role": 0,
    "hours": 3.0
  },
  {
    "desc": "Hotfix: Fix missing index on the users table that is causing the login endpoint to timeout",
    "prio": 3,
    "role": 2,
    "hours": 3.5
  },
  {
    "desc": "Setup multi-region active-active replication for the Redis cache cluster",
    "prio": 2,
    "role": 1,
    "hours": 35.0
  },
  {
    "desc": "Implement plaid API integration for automated bank account verification",
    "prio": 2,
    "role": 2,
    "hours": 28.0
  },
  {
    "desc": "Fix race condition in the optimistic UI update for the shopping cart counter",
    "prio": 1,
    "role": 0,
    "hours": 5.5
  },
  {
    "desc": "Hotfix: Update expired SSL certificate on the legacy payment gateway staging server",
    "prio": 3,
    "role": 1,
    "hours": 1.5
  },
  {
    "desc": "Write Terraform modules to provision elasticache Redis clusters with auto-scaling",
    "prio": 2,
    "role": 1,
    "hours": 16.0
  },
  {
    "desc": "Optimize WebGL shader compilation time to reduce initial load stalling on low-end devices",
    "prio": 2,
    "role": 2,
    "hours": 22.5
  },
  {
    "desc": "Create synthetic data generation script for training the NLP sentiment analysis model",
    "prio": 1,
    "role": 2,
    "hours": 14.0
  },
  {
    "desc": "Update privacy policy documentation to reflect new GDPR requirements for EU customers",
    "prio": 0,
    "role": 0,
    "hours": 4.0
  },
  {
    "desc": "Migrate core Android architecture from MVP to MVVM using Kotlin Coroutines and StateFlow",
    "prio": 2,
    "role": 2,
    "hours": 40.0
  },
  {
    "desc": "Configure WAF (Web Application Firewall) rules to block common SQL injection patterns",
    "prio": 1,
    "role": 1,
    "hours": 8.0
  },
  {
    "desc": "Implement 'pull-to-refresh' functionality on the mobile activity feed",
    "prio": 1,
    "role": 0,
    "hours": 6.5
  },
  {
    "desc": "Refactor monolithic CSS file into modular SCSS components using BEM methodology",
    "prio": 1,
    "role": 0,
    "hours": 24.0
  },
  {
    "desc": "Setup zero-downtime deployment pipeline for the Node.js microservices using blue-green strategy",
    "prio": 2,
    "role": 1,
    "hours": 32.0
  },
  {
    "desc": "Hotfix: Revert broken database migration that dropped the 'last_login' column",
    "prio": 3,
    "role": 2,
    "hours": 3.0
  },
  {
    "desc": "Implement object pooling for particle effects in the Unity game engine to reduce garbage collection spikes",
    "prio": 2,
    "role": 2,
    "hours": 18.0
  },
  {
    "desc": "Design and implement a rate-limiting middleware using Redis sliding window algorithm",
    "prio": 2,
    "role": 2,
    "hours": 15.0
  },
  {
    "desc": "Audit and upgrade out-of-date npm packages flagged by Snyk for high-severity vulnerabilities",
    "prio": 1,
    "role": 1,
    "hours": 12.0
  },
  {
    "desc": "Add custom Google Analytics tracking events for the multi-step checkout funnel",
    "prio": 1,
    "role": 0,
    "hours": 4.5
  },
  {
    "desc": "Build a dead-letter queue (DLQ) processing dashboard for the customer support team",
    "prio": 1,
    "role": 2,
    "hours": 20.0
  },
  {
    "desc": "Draft the quarterly SOC2 compliance review report and gather access logs",
    "prio": 0,
    "role": 1,
    "hours": 10.0
  },
  {
    "desc": "Optimize image assets using WebP format and setup lazy loading on the marketing landing page",
    "prio": 1,
    "role": 0,
    "hours": 7.0
  },
  {
    "desc": "Implement federated learning aggregation server for privacy-preserving model updates",
    "prio": 2,
    "role": 2,
    "hours": 38.0
  },
  {
    "desc": "Fix Bluetooth Low Energy (BLE) payload parsing bug causing truncated sensor readings in the IoT hub",
    "prio": 2,
    "role": 2,
    "hours": 11.5
  },
  {
    "desc": "Add unit tests to the date utility functions to handle edge cases with leap years and timezones",
    "prio": 1,
    "role": 1,
    "hours": 6.0
  },
  {
    "desc": "Hotfix: Increase memory limit on the PDF generation Lambda function to prevent OOM crashes",
    "prio": 3,
    "role": 1,
    "hours": 2.0
  },
  {
    "desc": "Setup Datadog APM tracing across the Python backend and React frontend boundaries",
    "prio": 2,
    "role": 1,
    "hours": 14.0
  },
  {
    "desc": "Implement spatial audio occlusion using raycasting for the VR application",
    "prio": 2,
    "role": 2,
    "hours": 26.5
  },
  {
    "desc": "Replace polling with Server-Sent Events (SSE) for real-time notification delivery",
    "prio": 1,
    "role": 2,
    "hours": 16.0
  },
  {
    "desc": "Document the new GraphQL schema mutations in the developer portal",
    "prio": 0,
    "role": 0,
    "hours": 5.0
  },
  {
    "desc": "Migrate on-premise Jira instance to Atlassian Cloud and remap user permissions",
    "prio": 1,
    "role": 1,
    "hours": 22.0
  },
  {
    "desc": "Fix accessibility issue where screen readers cannot interpret the custom date picker modal",
    "prio": 1,
    "role": 0,
    "hours": 8.5
  },
  {
    "desc": "Implement distributed locking using ZooKeeper to prevent duplicate cron job executions",
    "prio": 2,
    "role": 2,
    "hours": 19.0
  },
  {
    "desc": "Create a runbook for restoring the primary PostgreSQL database from a point-in-time snapshot",
    "prio": 1,
    "role": 1,
    "hours": 7.0
  },
  {
    "desc": "Hotfix: Patch the CORS configuration to allow requests from the new subdomain",
    "prio": 3,
    "role": 1,
    "hours": 1.0
  },
  {
    "desc": "Integrate Apple HealthKit to sync step count data into the fitness tracking app",
    "prio": 1,
    "role": 2,
    "hours": 24.0
  },
  {
    "desc": "Train and deploy a fastText model to automatically categorize incoming support tickets",
    "prio": 2,
    "role": 2,
    "hours": 30.0
  },
  {
    "desc": "Refactor legacy class components to functional components using React Hooks in the dashboard",
    "prio": 1,
    "role": 0,
    "hours": 18.0
  },
  {
    "desc": "Configure an AWS EventBridge rule to trigger lambda function on specific S3 object creation",
    "prio": 1,
    "role": 1,
    "hours": 5.5
  },
  {
    "desc": "Implement pathfinding algorithm (A*) for enemy AI navigation on dynamic grids",
    "prio": 2,
    "role": 2,
    "hours": 32.0
  },
  {
    "desc": "Fix UI bug where long text strings break the layout of the user profile card on mobile view",
    "prio": 0,
    "role": 0,
    "hours": 2.5
  },
  {
    "desc": "Audit and remove inactive AWS IAM users to enforce least privilege principles",
    "prio": 1,
    "role": 1,
    "hours": 4.0
  },
  {
    "desc": "Implement a retry mechanism with jitter for outbound webhooks failing with 5xx errors",
    "prio": 1,
    "role": 2,
    "hours": 12.0
  },
  {
    "desc": "Set up a standalone mock server using WireMock for frontend development against unfinished APIs",
    "prio": 1,
    "role": 1,
    "hours": 9.0
  },
  {
    "desc": "Hotfix: Rollback broken feature flag causing blank screens for users in the Canadian region",
    "prio": 3,
    "role": 1,
    "hours": 2.5
  },
  {
    "desc": "Write e2e tests using Cypress to cover the password reset flow and email token validation",
    "prio": 1,
    "role": 1,
    "hours": 14.0
  },
  {
    "desc": "Implement dark mode support for the iOS application using Semantic Colors",
    "prio": 1,
    "role": 0,
    "hours": 20.0
  },
  {
    "desc": "Migrate message broker from RabbitMQ to Kafka to support higher throughput event streaming",
    "prio": 2,
    "role": 2,
    "hours": 38.0
  },
  {
    "desc": "Optimize the Dockerfile for the Go backend service to reduce image size and build time using multi-stage builds",
    "prio": 1,
    "role": 1,
    "hours": 6.0
  },
  {
    "desc": "Implement an auto-save feature using debounced API calls in the rich text editor",
    "prio": 1,
    "role": 0,
    "hours": 10.5
  },
  {
    "desc": "Update localization strings for Japanese and Korean in the Android resource files",
    "prio": 0,
    "role": 0,
    "hours": 3.0
  },
  {
    "desc": "Debug memory leak in the C++ computer vision module during continuous frame processing",
    "prio": 2,
    "role": 2,
    "hours": 25.0
  },
  {
    "desc": "Configure Prometheus alerting for unusually high API latency (P99 > 500ms)",
    "prio": 1,
    "role": 1,
    "hours": 4.5
  },
  {
    "desc": "Implement two-factor authentication (2FA) recovery code generation and verification",
    "prio": 2,
    "role": 2,
    "hours": 18.0
  },
  {
    "desc": "Hotfix: Fix incorrect floating-point rounding logic causing a 1-cent discrepancy in the billing service",
    "prio": 3,
    "role": 2,
    "hours": 5.0
  },
  {
    "desc": "Design empty state illustrations for the 'No internet connection' and 'No search results' views",
    "prio": 0,
    "role": 0,
    "hours": 8.0
  },
  {
    "desc": "Rewrite the data ingestion pipeline in PySpark to handle partitioned Parquet files more efficiently",
    "prio": 2,
    "role": 2,
    "hours": 30.0
  },
  {
    "desc": "Implement pagination and sorting on the data table component to handle lists >10k items",
    "prio": 1,
    "role": 0,
    "hours": 12.0
  },
  {
    "desc": "Deploy a self-hosted instance of Metabase for internal business intelligence reporting",
    "prio": 1,
    "role": 1,
    "hours": 10.0
  },
  {
    "desc": "Implement biometric prompt (FaceID/TouchID) before allowing sensitive wire transfers",
    "prio": 2,
    "role": 2,
    "hours": 16.5
  },
  {
    "desc": "Fix glitch where character animation gets stuck in a loop after jumping near a wall",
    "prio": 1,
    "role": 0,
    "hours": 7.0
  },
  {
    "desc": "Update the README with local development instructions for Windows using WSL2",
    "prio": 0,
    "role": 0,
    "hours": 2.0
  },
  {
    "desc": "Hotfix: Clear malformed cache keys in Redis that are breaking the product recommendation feed",
    "prio": 3,
    "role": 1,
    "hours": 2.5
  },
  {
    "desc": "Implement robust error handling and fallback UI for the external weather API integration",
    "prio": 1,
    "role": 0,
    "hours": 6.5
  },
  {
    "desc": "Create an automated script to parse and visualize firmware memory usage map files",
    "prio": 1,
    "role": 1,
    "hours": 9.0
  },
  {
    "desc": "Setup multi-factor authentication requirement for all users accessing the VPN",
    "prio": 2,
    "role": 1,
    "hours": 8.0
  },
  {
    "desc": "Refactor the Redux store to use Redux Toolkit slices and simplify boilerplate code",
    "prio": 1,
    "role": 0,
    "hours": 22.0
  },
  {
    "desc": "Implement OCR pipeline using Tesseract to extract invoice numbers from scanned documents",
    "prio": 2,
    "role": 2,
    "hours": 35.0
  },
  {
    "desc": "Fix time zone parsing bug in the scheduling API that schedules appointments 1 hour early",
    "prio": 2,
    "role": 2,
    "hours": 6.0
  },
  {
    "desc": "Migrate static asset hosting from standard S3 to CloudFront distribution with Edge caching",
    "prio": 1,
    "role": 1,
    "hours": 12.5
  },
  {
    "desc": "Implement gesture-based swipe navigation between screens in the React Native app",
    "prio": 1,
    "role": 0,
    "hours": 14.0
  },
  {
    "desc": "Update the swagger definition to document the rate limit headers returned by the API",
    "prio": 0,
    "role": 0,
    "hours": 3.0
  },
  {
    "desc": "Hotfix: Revert UI change that accidentally hid the 'Submit Order' button on mobile Safari",
    "prio": 3,
    "role": 0,
    "hours": 1.5
  },
  {
    "desc": "Implement continuous delivery (CD) pipeline to automatically push Docker images to ECR",
    "prio": 1,
    "role": 1,
    "hours": 10.0
  },
  {
    "desc": "Integrate Google Maps API to display clustered map markers for available retail locations",
    "prio": 1,
    "role": 0,
    "hours": 15.0
  },
  {
    "desc": "Refactor authentication middleware to support both session cookies and JWT bearer tokens",
    "prio": 2,
    "role": 2,
    "hours": 18.5
  },
  {
    "desc": "Optimize physics collision detection matrix to ignore collisions between purely decorative objects",
    "prio": 1,
    "role": 2,
    "hours": 8.0
  },
  {
    "desc": "Add custom error boundaries in React to gracefully handle component crashes and report to Sentry",
    "prio": 1,
    "role": 0,
    "hours": 5.5
  },
  {
    "desc": "Create a data migration script to normalize the legacy 'address' field into separate street, city, zip columns",
    "prio": 2,
    "role": 2,
    "hours": 14.0
  },
  {
    "desc": "Document the Disaster Recovery (DR) procedures for the main MongoDB cluster",
    "prio": 0,
    "role": 1,
    "hours": 6.0
  },
  {
    "desc": "Hotfix: Increase the timeout limit on the third-party credit check API call",
    "prio": 3,
    "role": 2,
    "hours": 2.0
  },
  {
    "desc": "Implement automated database seeding script for spinning up local development environments",
    "prio": 1,
    "role": 1,
    "hours": 8.5
  },
  {
    "desc": "Fix focus trapping inside the navigation modal to ensure WCAG keyboard navigation compliance",
    "prio": 1,
    "role": 0,
    "hours": 4.0
  },
  {
    "desc": "Set up an ELK stack (Elasticsearch, Logstash, Kibana) for centralized log aggregation",
    "prio": 2,
    "role": 1,
    "hours": 24.0
  },
  {
    "desc": "Implement role-based access control (RBAC) filtering on the user listing GraphQL query",
    "prio": 2,
    "role": 2,
    "hours": 16.0
  },
  {
    "desc": "Create a dashboard widget to visualize historical temperature data from IoT sensors using D3.js",
    "prio": 1,
    "role": 0,
    "hours": 18.5
  },
  {
    "desc": "Audit and migrate deprecated AWS API calls in the infrastructure-as-code repository before end-of-life",
    "prio": 1,
    "role": 1,
    "hours": 15.0
  },
  {
    "desc": "Hotfix: Fix null pointer exception triggered when a user uploads a file with no extension",
    "prio": 3,
    "role": 2,
    "hours": 3.5
  },
  {
    "desc": "Implement push notifications for iOS using APNs and integrate handling in the AppDelegate",
    "prio": 1,
    "role": 2,
    "hours": 20.0
  },
  {
    "desc": "Refactor the email templating system to use Handlebars and externalize template files",
    "prio": 1,
    "role": 2,
    "hours": 12.0
  },
  {
    "desc": "Write a Python script to periodically trim historical log tables to maintain database size limits",
    "prio": 1,
    "role": 1,
    "hours": 6.5
  },
  {
    "desc": "Update the color palette in the Figma design system to improve general contrast ratios",
    "prio": 0,
    "role": 0,
    "hours": 4.5
  },
  {
    "desc": "Implement a semantic search endpoint using vector embeddings and Pinecone database",
    "prio": 2,
    "role": 2,
    "hours": 35.0
  },
  {
    "desc": "Fix bug in the C# inventory script where dropping an item doesn't remove it from the array",
    "prio": 1,
    "role": 2,
    "hours": 5.0
  },
  {
    "desc": "Migrate the frontend CI runner from GitHub Actions to GitLab CI to match company standards",
    "prio": 1,
    "role": 1,
    "hours": 11.0
  },
  {
    "desc": "Implement deep linking in the Android app to allow external links to open specific user profiles",
    "prio": 1,
    "role": 2,
    "hours": 14.5
  },
  {
    "desc": "Hotfix: Restart the stalled background worker queue processing video encodings",
    "prio": 3,
    "role": 1,
    "hours": 1.5
  },
  {
    "desc": "Optimize React re-renders on the complex dashboard view using useMemo and useCallback",
    "prio": 2,
    "role": 0,
    "hours": 16.0
  },
  {
    "desc": "Configure automated database schema validation tests to run on every pull request",
    "prio": 1,
    "role": 1,
    "hours": 9.0
  },
  {
    "desc": "Implement a bulk-upload feature for products using CSV parsing and background job queues",
    "prio": 2,
    "role": 2,
    "hours": 26.0
  },
  {
    "desc": "Update copyright years and version numbers across all public-facing footers and legal docs",
    "prio": 0,
    "role": 0,
    "hours": 1.0
  },
  {
    "desc": "Establish a VPN tunnel to the client's legacy on-premise system for secure data extraction",
    "prio": 2,
    "role": 1,
    "hours": 14.0
  }
]

df = pd.DataFrame(data)

# df must have: df["desc"], df["prio"], df["role"], df["hours"]
y = df["hours"].values
X_num = df[["prio", "role"]].values

# Path A: counts
cv = CountVectorizer(max_features=500, stop_words="english")
X_text_a = cv.fit_transform(df["desc"]).toarray()
X_a = np.hstack([X_num, X_text_a])
mae_a, rmse_a = eval_model(X_a, y)

# Path B: TF-IDF
tfidf = TfidfVectorizer(max_features=500, stop_words="english", norm="l2", use_idf=True)
X_text_b = tfidf.fit_transform(df["desc"]).toarray()
X_b = np.hstack([X_num, X_text_b])
mae_b, rmse_b = eval_model(X_b, y)

print("Path A (counts): MAE", mae_a, "RMSE", rmse_a)
print("Path B (tfidf):  MAE", mae_b, "RMSE", rmse_b)