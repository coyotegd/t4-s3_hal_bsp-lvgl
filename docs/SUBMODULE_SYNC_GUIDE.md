# Git Submodule Sync Guide

## Overview

This repository (t4-s3_hal_bsp-lvgl) is the **parent HAL/BSP repository**. The child repository [t4-s3_base-apps](https://github.com/coyotegd/t4-s3_base-apps) includes this repository as a git submodule at `external/hal_bsp/`.

When you make changes to this parent repository, you need to update the submodule reference in the child repository to ensure `git clone --recursive` downloads the latest code.

## Understanding Git Submodules

A git submodule is a reference to a specific commit in another repository. When you clone with `--recursive`, git:
1. Clones the child repository
2. Reads the submodule reference (which points to a specific commit SHA)
3. Clones the parent repository and checks out that specific commit

**Important:** The submodule reference must be explicitly updated in the child repository whenever you want it to point to newer parent commits.

## Workflow: Updating Parent Changes

### Step 1: Complete and Merge Parent Changes

First, ensure your changes in this parent repository are complete and merged to the main branch.

```bash
# In parent repository (t4-s3_hal_bsp-lvgl)
cd /path/to/t4-s3_hal_bsp-lvgl

# Make sure all changes are committed
git status

# Push your feature branch
git push origin your-feature-branch

# Create PR and merge to main branch (via GitHub)
# OR merge locally if you have permissions:
git checkout main
git pull origin main
git merge your-feature-branch
git push origin main

# Note the commit SHA after merging
git log -1 --format="%H"
```

### Step 2: Update Submodule Reference in Child Repository

Now update the child repository to point to the latest parent commit.

```bash
# Clone or navigate to child repository
cd /path/to/t4-s3_base-apps

# Initialize submodule if not already done
git submodule update --init --recursive

# Navigate into the submodule
cd external/hal_bsp

# Fetch latest changes from parent repository
git fetch origin

# Checkout the specific commit or branch you want
# Option A: Checkout main branch (recommended after merge)
git checkout main
git pull origin main

# Option B: Checkout specific commit SHA
# git checkout <commit-sha>

# Return to child repository root
cd ../..

# Check the status - you should see the submodule has changes
git status
# Output will show: modified:   external/hal_bsp (new commits)

# Stage the submodule update
git add external/hal_bsp

# Commit the submodule reference update
git commit -m "Update hal_bsp submodule to latest version

Includes:
- Memory leak fixes in UI components
- NULL check improvements in HAL/BSP
- Best practices implementation
"

# Push the changes
git push origin main  # or your feature branch
```

### Step 3: Verify the Update

Test that a fresh clone gets the correct version:

```bash
# In a temporary directory
cd /tmp
git clone --recursive https://github.com/coyotegd/t4-s3_base-apps.git test-clone
cd test-clone/external/hal_bsp

# Verify the commit SHA matches what you expect
git log -1 --oneline

# Verify you can build with the new parent code
cd ../..
idf.py build
```

## Quick Reference Commands

### Update submodule to latest main branch:
```bash
cd /path/to/t4-s3_base-apps
git submodule update --remote external/hal_bsp
git add external/hal_bsp
git commit -m "Update hal_bsp submodule to latest"
git push
```

### Update submodule to specific commit:
```bash
cd /path/to/t4-s3_base-apps/external/hal_bsp
git fetch
git checkout <commit-sha>
cd ../..
git add external/hal_bsp
git commit -m "Update hal_bsp submodule to <commit-sha>"
git push
```

### Check current submodule commit:
```bash
cd /path/to/t4-s3_base-apps
git submodule status
# Shows: <commit-sha> external/hal_bsp (<branch-or-tag>)
```

## Common Scenarios

### Scenario 1: You merged parent changes to main

**Parent (t4-s3_hal_bsp-lvgl):**
1. Merge your PR to `main`
2. Note the new commit SHA

**Child (t4-s3_base-apps):**
```bash
cd t4-s3_base-apps
git submodule update --remote external/hal_bsp
git add external/hal_bsp
git commit -m "Update hal_bsp to include [describe changes]"
git push origin main
```

### Scenario 2: You want to test parent changes before merging

**Parent:** Keep changes on feature branch

**Child:** 
```bash
cd t4-s3_base-apps/external/hal_bsp
git fetch origin
git checkout origin/copilot/review-ui-components-drivers
cd ../..
git add external/hal_bsp
git commit -m "Test hal_bsp feature branch"
# Push to test branch, not main
```

### Scenario 3: Multiple developers working simultaneously

**Best Practice:**
1. Always update parent first, merge to main
2. Communicate the new parent commit SHA to team
3. Each developer updates their child repository
4. Coordinate merges to child repository

## Troubleshooting

### Problem: "git clone --recursive" gets old parent code

**Cause:** Child repository's submodule reference wasn't updated.

**Solution:** Follow Step 2 above to update the submodule reference and push.

### Problem: Submodule shows as "modified" but I didn't change it

**Cause:** The submodule is checked out at a different commit than what's referenced.

**Solution:**
```bash
cd t4-s3_base-apps/external/hal_bsp
git status  # See what commit you're on
git log --oneline -5  # See recent commits

# Either commit the new reference:
cd ../..
git add external/hal_bsp
git commit -m "Update submodule reference"

# Or reset to the referenced commit:
git submodule update external/hal_bsp
```

### Problem: Submodule is in "detached HEAD" state

**This is normal!** Submodules are typically in detached HEAD because they point to specific commits, not branches.

**If you need to make changes:**
```bash
cd external/hal_bsp
git checkout main  # or create a branch
# Make changes, commit, push
# Then update parent repository and return here to update reference
```

## Best Practices

### 1. Always Use Main Branch References (When Stable)

Point submodules to commits on the `main` branch for stability:
```bash
cd external/hal_bsp
git checkout main
git pull
```

### 2. Tag Important Releases

In parent repository:
```bash
git tag -a v1.0.0 -m "Release 1.0.0: Memory leak fixes"
git push origin v1.0.0
```

In child repository:
```bash
cd external/hal_bsp
git fetch --tags
git checkout v1.0.0
cd ../..
git add external/hal_bsp
git commit -m "Update hal_bsp to v1.0.0"
```

### 3. Document What Changed

When updating submodule reference, include meaningful commit message:
```bash
git commit -m "Update hal_bsp submodule

Changes included:
- Fixed memory leaks in UI components
- Added NULL checks in HAL
- Improved error handling in WiFi manager

Ref: parent-repo-commit-sha or PR number
"
```

### 4. Test After Updating

Always build and test after updating submodules:
```bash
idf.py fullclean
idf.py build
idf.py flash monitor  # if testing on hardware
```

### 5. Keep Submodules Updated Regularly

Don't let submodules get too far behind. Regular small updates are easier to manage than large infrequent ones.

## Automation Options

### GitHub Actions (Advanced)

You can automate submodule updates with GitHub Actions:

```yaml
# In child repository: .github/workflows/update-submodule.yml
name: Update HAL Submodule
on:
  workflow_dispatch:
  schedule:
    - cron: '0 0 * * 1'  # Weekly on Monday

jobs:
  update:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
        with:
          submodules: true
          token: ${{ secrets.GITHUB_TOKEN }}
      
      - name: Update submodule
        run: |
          git submodule update --remote external/hal_bsp
          
      - name: Create Pull Request
        uses: peter-evans/create-pull-request@v5
        with:
          commit-message: "Update hal_bsp submodule"
          title: "Automated: Update HAL/BSP submodule"
          branch: update-hal-submodule
```

## Additional Resources

- [Git Submodules Documentation](https://git-scm.com/book/en/v2/Git-Tools-Submodules)
- [GitHub Submodules Guide](https://github.blog/2016-02-01-working-with-submodules/)
- [Pro Git: Submodules Chapter](https://git-scm.com/book/en/v2/Git-Tools-Submodules)

## Questions?

If you have questions about this process, please:
1. Check the troubleshooting section above
2. Review your git status and submodule status
3. Open an issue in the repository with details about your specific situation
