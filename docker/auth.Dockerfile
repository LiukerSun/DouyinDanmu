FROM node:22-bookworm-slim
WORKDIR /app
COPY auth/server.js ./server.js
COPY auth/portable-web.js ./portable-web.js
RUN mkdir /data && chown node:node /data
USER node
EXPOSE 8091
CMD ["node", "server.js"]
