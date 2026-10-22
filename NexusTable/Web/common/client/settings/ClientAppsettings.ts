/** Settings the browser can see (NEXT_PUBLIC_*), read once. */
const ClientAppsettings = {
  ApiUrl: (process.env.NEXT_PUBLIC_API_URL ?? "http://localhost:5240").replace(/\/$/, ""),
  DevLogin: process.env.NEXT_PUBLIC_DEV_LOGIN === "true",
  IsDevelopment: process.env.NODE_ENV !== "production",
};

export default ClientAppsettings;
