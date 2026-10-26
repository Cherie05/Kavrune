# Working on Kavrune

Before changing a feature or declaring work complete, read [PRODUCT_STANDARDS.md](PRODUCT_STANDARDS.md) and the relevant acceptance cases in [TEST_PLAN.md](TEST_PLAN.md). Read [PRODUCT_REQUIREMENTS.md](PRODUCT_REQUIREMENTS.md) for scope and [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md) before preparing artifacts.

Keep this an offline native Linux utility. Preserve explicit target selection, system-drive protection, source/device identity checks, mandatory verification and fail-closed authorization. Never run automated destructive tests against attached physical drives; use generated temporary regular files or a separately authorized disposable test environment.

Implement real controls, useful errors and accessible labels. Keep policies synchronized with actual behavior. Run relevant checks and distinguish implemented code from observed validation, unsupported features and unverified release gates. A passing build never establishes universal hardware, legal, security or enterprise certification. Do not add accounts, cloud services, telemetry, update downloads or forced license consent to satisfy inapplicable generic requirements.
