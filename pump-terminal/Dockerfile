# ---- Build ----
FROM node:22-alpine AS build
WORKDIR /app
COPY client/package*.json client/
COPY server/package*.json server/
RUN npm --prefix client ci && npm --prefix server ci
COPY client client
COPY server server
RUN npm --prefix client run build && npm --prefix server run build && npm --prefix server prune --omit=dev

# ---- Run ----
FROM node:22-alpine
ENV NODE_ENV=production PORT=8080 DATA_DIR=/data STATIC_DIR=/app/client/dist
WORKDIR /app
COPY --from=build /app/server/node_modules server/node_modules
COPY --from=build /app/server/dist server/dist
COPY --from=build /app/server/package.json server/package.json
COPY --from=build /app/client/dist client/dist
VOLUME /data
EXPOSE 8080
WORKDIR /app/server
CMD ["node", "dist/index.js"]
