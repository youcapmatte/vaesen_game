# Nexus Table on AWS

An AWS CDK app (C#) that runs the Nexus Table at **https://nexus.norse.games**, with the API at **https://api.nexus.norse.games**, in `eu-central-1`.

| Stack | What it holds |
|---|---|
| `NexusTableDns` | The Route 53 hosted zone `nexus.norse.games`. `norse.games` stays at Namecheap, which delegates this subdomain with four NS records. |
| `NexusTable` | The DynamoDB table `nexus-table-prod` (kept if the stack is deleted), a VPC with public subnets (no NAT gateway), the HTTPS certificate, one load balancer, the ECS cluster `nexus-table` with three Fargate services, an EFS file system for the mystery engine's files (kept if the stack is deleted), and the two DNS records. |

| Service | Image | Size | Reached at |
|---|---|---|---|
| `nexus-table-api` | `NexusTable/Api/Dockerfile` | 0.5 vCPU, 1 GB | `api.nexus.norse.games` (health check `/health`); case files and worlds on EFS at `/data` |
| `nexus-table-web` | `NexusTable/Web/Dockerfile` | 0.25 vCPU, 512 MB | `nexus.norse.games` |
| `nexus-gm` | `NexusTable/GmAgent/Dockerfile` | 0.5 vCPU, 1 GB | nothing: it only connects out |

CDK builds the images with Docker on your machine and pushes them to AWS during the deploy.

**Settings** that aren't secret are in [cdk.json](cdk.json) under `context`: the domains, the table name, the Auth0 domain and client id, the hosts (`hostSubjects`, comma-separated Auth0 user ids) and, optionally, the ElevenLabs voices.

**Keys** live in one Secrets Manager secret, `nexus-table/prod`. You keep them in `secrets.prod.json` (ignored by git), and `deploy.ps1` uploads it on every deploy.

## You need

- The AWS CLI with the `vaesen` profile signed in: `aws sso login --profile vaesen`
- Docker Desktop, running
- Node (for `npx aws-cdk`) and the .NET 10 SDK

## The first deploy

Run everything from PowerShell in this folder (`NexusTable/Infra`).

1. **Keys.** Copy `secrets.example.json` to `secrets.prod.json` and fill it in: the same LiveKit, Deepgram, Anthropic and ElevenLabs keys as in `GmAgent/.env.local` (or separate production keys), and the Auth0 client secret. Leave `NEXUS_AGENT_KEY` and `AUTH0_SECRET` empty; the script generates them once and saves them in the file.
2. **Prepare CDK**, once for the account and region:
   ```powershell
   .\deploy.ps1 -Bootstrap
   ```
3. **Create the hosted zone:**
   ```powershell
   .\deploy.ps1 -Dns
   ```
   It prints four name servers. At Namecheap, open **Domain List > norse.games > Manage > Advanced DNS** and add four records of type **NS Record**, host `nexus`, one per name server. Wait until `Resolve-DnsName nexus.norse.games -Type NS` answers with them (usually minutes).
4. **Auth0.** In **Applications > Nexus Table Web > Settings**, add the production addresses next to the localhost ones (comma-separated), then save:
   - Allowed Callback URLs: `https://nexus.norse.games/auth/callback`
   - Allowed Logout URLs: `https://nexus.norse.games`
   - Allowed Web Origins: `https://nexus.norse.games`
5. **Deploy:**
   ```powershell
   .\deploy.ps1
   ```
   CDK lists the IAM and security group changes and asks for a `y`. The first deploy takes about 15–20 minutes, most of it the certificate and the first image builds. Then open https://nexus.norse.games and sign in with Discord: you're the host from the first sign-in.

## Later

- **Deploy a new version** (code, settings or keys): `.\deploy.ps1`. Only changed images are rebuilt, and the services restart one task at a time.
- **Change a key:** edit `secrets.prod.json`, then `.\deploy.ps1`.
- **Read the logs:**
  ```powershell
  aws logs tail /nexus-table/nexus-table-api --follow --profile vaesen --region eu-central-1
  ```
  The other two are `/nexus-table/nexus-table-web` and `/nexus-table/nexus-gm`.

## Cost

Roughly $80 a month while all three services run (approximate, eu-central-1, not checked against today's prices). The load balancer and the public IPv4 addresses are about half of it, and the containers most of the rest; DynamoDB, Secrets Manager and Route 53 cost a few dollars at most.

## What isn't covered

- Local data (the JSON files in `.data`, including case files and worlds) isn't copied: production starts empty.
- Production generates mysteries from the draft content (`Mystery__UseDraftContent=true`), since none is approved yet. Turn it off in `AppStack.cs` once content is reviewed.
- One task per service. More API tasks need sticky sessions on the load balancer for SignalR first.
