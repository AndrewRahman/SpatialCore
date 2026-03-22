# Conductor — Complete Setup Guide for Spatial Media Library

Step-by-step instructions for getting [Conductor](https://www.conductor.build/) up and running to manage multi-plugin development across the Spatial Media Library suite.

**Status:** Setup complete as of 2026-03-22. Two repos added (OpenSpatialDelay, SpatialCore), three workspaces tested and working.

**Important:** Conductor requires repos to have a GitHub remote (`origin`). If adding a local-only project, you must first:
1. `git init && git add -A && git commit -m "Initial commit"`
2. `gh repo create RepoName --private --source=. --push`
Then add it to Conductor.

---

## Glossary

| Term | Meaning |
|---|---|
| **Workspace** | An isolated copy of your repo on its own git branch. Each workspace has its own Claude agent. Changes in one workspace don't affect others. |
| **Branch** | A parallel version of your code. Like a copy you can edit freely without touching the original. |
| **PR (Pull Request)** | A GitHub feature that lets you propose merging changes from one branch into another (usually into `main`). It's a review step between "work in progress" and "officially part of the project." For solo work, PRs are optional — you can merge branches directly. |
| **Diff** | A side-by-side view showing exactly what lines were added, removed, or changed. |
| **Merge** | Combining changes from one branch into another (e.g., merging your workspace branch into `main`). |

---

## Prerequisites

Before installing Conductor, make sure these two things are set up:

### 1. GitHub CLI must be authenticated

Open Terminal and run:
```
gh auth status
```
If it says "Logged in to github.com", you're good. If not, run:
```
gh auth login
```
and follow the prompts.

### 2. Claude Code must be logged in

If you haven't already, run:
```
claude /login
```

---

## Step 1: Download & Install Conductor

1. Go to [conductor.build](https://www.conductor.build/)
2. Click **"Download Conductor"**
3. Drag the app into your **Applications** folder
4. Open Conductor from Applications

> Note: Conductor is Mac-only right now. No Windows/Linux yet.

---

## Step 2: Add Your Repository

When Conductor opens, you need to add a repo. You have two options:

- **Import from a local folder** — point it at your existing project (e.g., `~/Desktop/CLAUDE/Project/OpenSpatialDelay`)
- **Import from a Git URL** — paste a GitHub repo URL

For the Spatial Media Library, add each plugin repo you want to work on (OpenSpatialDelay, SpatialCore, future plugins, etc.).

---

## Step 3: Create Your First Workspace

Once you add a repo, Conductor automatically creates a workspace. Key things to know:

- Each workspace is an **isolated copy** of your repo on its own branch
- Only git-tracked files are copied by default
- You can create more workspaces anytime with **Cmd+Shift+N** or by clicking the **+** button next to "Workspaces"

You can also create workspaces from:
- Specific git branches
- GitHub pull requests
- Linear issues

---

## Step 4: Configure a Setup Script (Optional but Recommended)

Click your **repo name in the sidebar** to open Repository Settings. Under Scripts, add a setup script that runs when each new workspace is created. For JUCE projects:

```bash
# Install/configure dependencies for the workspace
export PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:$PATH"
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

Two environment variables are available in scripts:
- `$CONDUCTOR_WORKSPACE_PATH` — the active workspace directory
- `$CONDUCTOR_ROOT_PATH` — the original repository root

---

## Step 5: Configure a Run Script (Optional)

Also in Repository Settings, you can set a run script for background processes (dev servers, test watchers, etc.). Conductor allocates 10 ports per workspace starting at `$CONDUCTOR_PORT`.

For JUCE audio plugin projects you may not need this, but it's available if you add a web UI or test server later.

---

## Step 6: Run Multiple Agents in Parallel

This is the core feature. To work on multiple things at once:

1. **Press Cmd+N** or click **+** next to "Workspaces" to create a new workspace
2. Each workspace gets its own isolated Claude Code agent
3. Type your instructions in each workspace's chat — e.g.:
   - Workspace 1: "Implement the chorus DSP engine"
   - Workspace 2: "Build the tremolo UI layout"
4. Each Claude works independently without interfering with the others
5. All changes stay on separate branches until you explicitly merge

---

## Step 7: Review Changes

When an agent finishes:

1. Click the **"Changes"** tab (top-right of the workspace view) to see what the agent modified
2. Open the **Diff Viewer** to see exactly what lines were added/removed/changed
3. Conductor recommends actions for each change
4. Review the diffs, ask questions, request modifications
5. **Open in your IDE** with **Cmd+O** if you want to inspect further

---

## Step 8: Sync to GitHub & Create PRs

Once you're happy with the changes:

1. Conductor guides you through syncing to GitHub
2. It recommends each step toward merging your PR (pull request)
3. Each workspace's branch becomes a PR you can review and merge independently
4. For solo work, you can also merge branches directly without creating a PR

---

## Quick Reference — Keyboard Shortcuts

| Action | Shortcut |
|---|---|
| New workspace | **Cmd+N** |
| New workspace (with options) | **Cmd+Shift+N** |
| Open workspace in IDE | **Cmd+O** |
| Switch between workspaces | **Cmd+1**, **Cmd+2**, etc. |

---

## Current Conductor Setup (as of 2026-03-22)

| Repo | GitHub Remote | Workspaces |
|---|---|---|
| **OpenSpatialDelay** | `github.com/AndrewRahman/OpenSpatialDelay` (private) | Valletta (Cmd+1), Brisbane (Cmd+2) |
| **SpatialCore** | `github.com/AndrewRahman/SpatialCore` (private) | Tyler (Cmd+3) |

Both repos are added to Conductor and verified working.

---

## Multi-Plugin Workflow for Spatial Media Library

For working on multiple plugins simultaneously:

1. Add each plugin repo to Conductor (OpenSpatialDelay, SpatialCore, future plugins)
2. Create a workspace per task — e.g., "chorus DSP", "tremolo UI", "SpatialCore refactor"
3. Give each workspace its instructions
4. Let them all run in parallel
5. Review changes in each workspace when done
6. Merge branches or create PRs when ready

---

## Sources

- [Conductor Docs](https://docs.conductor.build/)
- [Conductor Website](https://www.conductor.build/)
- [Installation Guide](https://docs.conductor.build/installation.md)
- [First Workspace Guide](https://docs.conductor.build/first-workspace.md)
- [Workspace Setup Guide](https://docs.conductor.build/guides/how-to-setup.md)
- [Parallel Agents](https://docs.conductor.build/core/parallel-agents.md)
- [Diff Viewer](https://docs.conductor.build/core/diff-viewer.md)
