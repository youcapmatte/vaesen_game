# The Nexus Table Gamemaster agent. Build from this folder:
#   docker build -t nexus-gm .
# Pass the .env.example settings as environment variables (LIVEKIT_*, DEEPGRAM_API_KEY, ANTHROPIC_API_KEY,
# ELEVEN_API_KEY, NEXUS_API_URL, NEXUS_AGENT_KEY). It only connects out; nothing needs to reach it.
# The worker answers health checks on 8081.

FROM ghcr.io/astral-sh/uv:python3.12-bookworm-slim
ENV UV_COMPILE_BYTECODE=1 \
    UV_LINK_MODE=copy \
    PYTHONUNBUFFERED=1
WORKDIR /app

# Dependencies first, from the lock file, so the layer is reused until uv.lock changes.
COPY pyproject.toml uv.lock .python-version ./
RUN uv sync --locked --no-dev

COPY agent.py ./
COPY nexus_gm/ nexus_gm/

RUN useradd --create-home --uid 10001 agent
USER agent
ENV PATH="/app/.venv/bin:$PATH"
EXPOSE 8081
CMD ["python", "agent.py", "start"]
