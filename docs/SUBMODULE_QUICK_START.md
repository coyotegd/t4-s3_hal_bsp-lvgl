# Quick Start: Syncing Parent to Child Repository

## TL;DR - The Essential Commands

After making changes to this parent repository and merging to `main`, update the child:

```bash
# Navigate to child repository
cd /path/to/t4-s3_base-apps

# Update submodule to latest parent commit
git submodule update --remote external/hal_bsp

# Commit the submodule reference update
git add external/hal_bsp
git commit -m "Update hal_bsp submodule to latest"

# Push to GitHub
git push origin main
```

**Done!** Now `git clone --recursive` will get the latest parent code.

---

## Visual Workflow

```
┌─────────────────────────────────────────────────────────┐
│  PARENT REPO: t4-s3_hal_bsp-lvgl                       │
│  Status: Changes merged to main ✓                       │
│  Latest commit: abc123                                  │
└─────────────────────────────────────────────────────────┘
                        │
                        │ git submodule update --remote
                        ▼
┌─────────────────────────────────────────────────────────┐
│  CHILD REPO: t4-s3_base-apps                            │
│  external/hal_bsp/ → points to commit: xyz789 (old)    │
│                                                          │
│  After update:                                          │
│  external/hal_bsp/ → points to commit: abc123 ✓        │
└─────────────────────────────────────────────────────────┘
                        │
                        │ git commit + push
                        ▼
┌─────────────────────────────────────────────────────────┐
│  GITHUB: t4-s3_base-apps                                │
│  Submodule reference updated in repository ✓            │
│                                                          │
│  Anyone cloning now gets parent commit: abc123          │
└─────────────────────────────────────────────────────────┘
```

---

## Why Do I Need to Do This?

Git submodules are **commit pointers**, not branch references:

- ❌ **NOT like this**: "Use whatever is on main branch"
- ✅ **Actually like this**: "Use commit abc123 specifically"

When you:
```bash
git clone --recursive https://github.com/coyotegd/t4-s3_base-apps.git
```

Git:
1. Clones the child repository
2. Reads the submodule pointer (e.g., "xyz789")
3. Clones the parent and checks out commit xyz789
4. **Ignores** any newer commits on main

To update the pointer from "xyz789" to "abc123", you must:
- Navigate to the child
- Update the submodule
- Commit the new pointer
- Push

---

## Verify It Worked

```bash
# Check the submodule points to the right commit
cd /path/to/t4-s3_base-apps
git submodule status

# Output should show:
#  abc123 external/hal_bsp (heads/main)
#  ^^^^^^ This should match parent's latest commit

# Test a fresh clone
cd /tmp
git clone --recursive https://github.com/coyotegd/t4-s3_base-apps.git test
cd test/external/hal_bsp
git log -1 --oneline
# Should show abc123
```

---

## Common Mistakes

### ❌ Mistake 1: Only pushing parent changes
```bash
# In parent repo
git push origin main  # ✓ Parent updated

# Stop here ✗ Child still points to old commit!
```

**Fix**: Also update child as shown above.

### ❌ Mistake 2: Forgetting to commit submodule update
```bash
cd t4-s3_base-apps
git submodule update --remote external/hal_bsp
# Stop here ✗ Change not committed!
```

**Fix**: Add + commit + push the change.

### ❌ Mistake 3: Not pushing the child
```bash
cd t4-s3_base-apps
git submodule update --remote external/hal_bsp
git add external/hal_bsp
git commit -m "Update submodule"
# Stop here ✗ Only local, not on GitHub!
```

**Fix**: `git push origin main`

---

## For More Details

See the [complete Submodule Sync Guide](SUBMODULE_SYNC_GUIDE.md) for:
- Detailed explanations
- Troubleshooting
- Multiple scenarios
- Best practices
- Automation options
