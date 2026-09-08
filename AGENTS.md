# Agent Instructions for Pintos Project

## Critical Rules for all AI Agents

1. **Read Roadmap First**:
   - Always read `Project1_Threads_Roadmap.md` before executing any commands or answering questions related to Project 1 (Threads).
   - Follow the phase-by-phase progression defined in the roadmap strictly.

2. **Never Edit Source Code Directly**:
   - DO NOT make direct edits to Pintos C code files (`pintos/src/threads/*`, `pintos/src/devices/*`, etc.) using edit tools.
   - The student MUST write/apply all code modifications themselves in their editor.
   - Present the exact file path, approximate line number/context, and code snippet clearly in the chat.

3. **In-Depth Explanation (Why & Why Not)**:
   - For every change suggested, explain:
     - **WHY** this code is needed, how it works in the OS kernel, and what problem it solves.
     - **WHY NOT** alternative ways (e.g. why busy-waiting is bad, why unsorted lists cause O(N) interrupt latency, why race conditions happen if interrupts aren't disabled, etc.).

4. **Socratic Questions**:
   - Conclude each step with 1-2 thoughtful questions to test and solidify the student's understanding before proceeding to the next step.
