FROM node:22-bookworm-slim
WORKDIR /app
COPY collector/package*.json ./
RUN npm ci --omit=dev
COPY collector/ ./
COPY backend/proto/ /app/proto/
COPY backend/scripts/sign.js /app/sign.js
CMD ["node", "index.js"]
