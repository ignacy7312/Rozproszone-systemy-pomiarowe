FROM node:22-alpine

WORKDIR /app

ENV NODE_ENV=production
ENV PORT=5173
ENV API_TARGET=http://flask:5001

COPY public ./public
COPY server.mjs ./server.mjs
COPY package.json ./package.json

USER node

EXPOSE 5173

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD node -e "fetch('http://127.0.0.1:' + (process.env.PORT || 5173) + '/').then(r => process.exit(r.ok ? 0 : 1)).catch(() => process.exit(1))"

CMD ["node", "server.mjs"]