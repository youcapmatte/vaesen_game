# The Nexus Table web app. Build from this folder:
#   docker build -t nexus-table-web --build-arg NEXT_PUBLIC_API_URL=https://api.example.com .
# NEXT_PUBLIC_* values are baked into the browser code at build time. The Auth0 values are read at runtime:
# pass APP_BASE_URL, AUTH0_DOMAIN, AUTH0_CLIENT_ID, AUTH0_CLIENT_SECRET, AUTH0_SECRET, AUTH0_AUDIENCE to the container.
# Listens on 3030.

FROM node:22-alpine AS deps
WORKDIR /app
COPY package.json package-lock.json ./
RUN npm ci

FROM node:22-alpine AS build
WORKDIR /app
ARG NEXT_PUBLIC_API_URL
ARG NEXT_PUBLIC_DEV_LOGIN=false
RUN test -n "$NEXT_PUBLIC_API_URL" || (echo "Pass --build-arg NEXT_PUBLIC_API_URL=<the API's public URL>" && exit 1)
ENV NEXT_PUBLIC_API_URL=$NEXT_PUBLIC_API_URL \
    NEXT_PUBLIC_DEV_LOGIN=$NEXT_PUBLIC_DEV_LOGIN \
    NEXT_TELEMETRY_DISABLED=1
COPY --from=deps /app/node_modules ./node_modules
COPY . .
RUN npm run build

# The standalone server (next.config.mjs: output "standalone") with only the modules it uses.
FROM node:22-alpine
WORKDIR /app
ENV NODE_ENV=production \
    NEXT_TELEMETRY_DISABLED=1 \
    HOSTNAME=0.0.0.0 \
    PORT=3030
COPY --from=build --chown=node:node /app/.next/standalone ./
COPY --from=build --chown=node:node /app/.next/static ./.next/static
USER node
EXPOSE 3030
CMD ["node", "server.js"]
