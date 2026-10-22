/** @type {import('next').NextConfig} */
const nextConfig = {
    // A Node server that can be hosted anywhere (on AWS: a container or App Runner).
    output: "standalone",
    experimental: {
        optimizePackageImports: ["@mantine/core", "@mantine/hooks", "@tabler/icons-react"],
    },
    images: {
        remotePatterns: [{ protocol: "https", hostname: "cdn.discordapp.com", pathname: "/**" }],
    },
};

export default nextConfig;
