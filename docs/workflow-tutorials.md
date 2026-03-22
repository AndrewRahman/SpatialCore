# Spatial Media Library — Multi-Plugin Development Tutorials

Step-by-step guides for working on multiple plugins simultaneously using Claude Code. Written for non-technical users.

---

## Tutorial 1: The Conductor Pattern

**What is it?** You open one Claude Code session (the "conductor") that manages work across multiple plugin projects.

**When to use it:** When you want to work on 2-3 plugins at the same time within a single conversation.

### Step-by-Step

**Step 1: Open your main project folder**

Open Terminal (the black icon with `>_` in your Applications folder) and type:
```
cd ~/Desktop/CLAUDE
claude
```
This starts Claude Code. This session is your **conductor**.

**Step 2: Tell the conductor what you want**

Type something like:
```
I want to work on OpenSpatialChorus and OpenSpatialTremolo at the same time.
Set up both projects and start implementing the DSP engine for each.
```

**Step 3: The conductor creates isolated workspaces**

Claude creates two separate copies of your code. Each agent works in its own folder and can't interfere with the other. Think of it like having two separate desks — each with its own copy of the blueprints.

**Step 4: Review results**

When each agent finishes, the conductor summarizes what was done. You review the changes, ask questions, and decide what to keep.

**Step 5: Merge the work**

Each agent creates changes on its own branch. The conductor can help you merge those branches back when you're happy.

### Key Concepts
- **Conductor** = your main Claude session. It plans, delegates, and reviews.
- **Agents** = helper Claude sessions that do the actual coding. Each gets their own isolated copy.
- **Worktree** = a separate working folder created from your repository. Changes in one worktree don't affect others.
- You never lose work — everything is on its own branch until you explicitly merge.

---

## Tutorial 2: The Worktree Pattern

**What is it?** You open multiple Terminal windows, each running Claude Code in a different plugin folder.

**When to use it:** When each plugin has its own folder and you want full independence.

### Step-by-Step

**Step 1: Set up your folder structure**

Create a folder for all your plugins:
```
mkdir -p ~/Desktop/SpatialMediaLab
cd ~/Desktop/SpatialMediaLab
```

**Step 2: Create folders for each plugin**

```
mkdir OpenSpatialChorus
cd OpenSpatialChorus
git init
cd ..

mkdir OpenSpatialTremolo
cd OpenSpatialTremolo
git init
cd ..
```

**Step 3: Open separate Terminal windows**

- **Window 1:** File > New Window in Terminal
  ```
  cd ~/Desktop/SpatialMediaLab/OpenSpatialChorus
  claude
  ```

- **Window 2:** File > New Window again
  ```
  cd ~/Desktop/SpatialMediaLab/OpenSpatialTremolo
  claude
  ```

Now you have two independent Claude sessions. Switch between them by clicking on the window.

**Step 4: Work in each window independently**

In Window 1: "Set up a JUCE plugin for a spatial chorus effect"
In Window 2: "Set up a JUCE plugin for a spatial tremolo effect"

Both work at the same time. Changes in one don't affect the other.

### Tips
- **Switch windows:** Click on the one you want, or press Cmd+` (backtick)
- **Stop Claude:** Press Escape in that window
- **Resume later:** Open Terminal, `cd` to the folder, type `claude`

---

## Tutorial 3: The GitTree Pattern

**What is it?** You work on multiple features of the same plugin using git worktrees. Each feature gets its own folder and branch.

**When to use it:** When you want to build multiple features of one plugin at the same time (e.g., oscillator engine AND filter engine of the synthesizer).

### Step-by-Step

**Step 1: Navigate to your plugin**

```
cd ~/Desktop/SpatialMediaLab/OpenSpatialSynthesizer
claude
```

**Step 2: Create a worktree**

Type:
```
Start a worktree called "oscillator-engine"
```

Claude creates a separate folder with its own branch. You're now working in this isolated copy.

**Step 3: Do your work**

Tell Claude what to build. All changes happen in the worktree, not in your main code.

**Step 4: When you're done**

Type:
```
Exit the worktree and keep the changes
```

You're back in your main folder. The worktree branch is preserved for merging later.

**Step 5: Start another worktree**

```
Start a worktree called "filter-engine"
```

Now you're in a new isolated copy. The oscillator work is safe on its own branch.

### What Happens to the Branches

```
worktree-oscillator-engine  — has your oscillator changes
worktree-filter-engine      — has your filter changes
main                        — untouched original code
```

When both are ready, merge them into main one at a time.

---

## Which Pattern Should I Use?

| I want to... | Use this |
|-------------|----------|
| Work on 2-3 plugins at once, one conversation | **Conductor** |
| Work on plugins in completely separate sessions | **Worktree** |
| Build multiple features of ONE plugin | **GitTree** |
| Quick prototype across several plugins | **Conductor** |
| Deep focused work on one plugin | **GitTree** |
