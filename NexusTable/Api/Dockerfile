# The Nexus Table API. Build from the repository root, since the API references VaesenAPI/Framework:
#   docker build -f NexusTable/Api/Dockerfile -t nexus-table-api .
# Listens on 8080. Settings come from environment variables, e.g. Auth0__Domain, LiveKit__ApiSecret, Cors__Origins__0.

FROM mcr.microsoft.com/dotnet/sdk:10.0 AS build
WORKDIR /src

# Restore first, so the package layer is reused until a project file changes.
COPY VaesenAPI/Framework/Vaesen.Framework.Core/Vaesen.Framework.Core.csproj VaesenAPI/Framework/Vaesen.Framework.Core/
COPY NexusTable/Api/Nexus.Table/Nexus.Table.csproj NexusTable/Api/Nexus.Table/
COPY NexusTable/Api/Nexus.Table.Api/Nexus.Table.Api.csproj NexusTable/Api/Nexus.Table.Api/
RUN dotnet restore NexusTable/Api/Nexus.Table.Api/Nexus.Table.Api.csproj

COPY VaesenAPI/Framework/Vaesen.Framework.Core/ VaesenAPI/Framework/Vaesen.Framework.Core/
COPY NexusTable/Api/Nexus.Table/ NexusTable/Api/Nexus.Table/
COPY NexusTable/Api/Nexus.Table.Api/ NexusTable/Api/Nexus.Table.Api/
RUN dotnet publish NexusTable/Api/Nexus.Table.Api/Nexus.Table.Api.csproj -c Release -o /app --no-restore

FROM mcr.microsoft.com/dotnet/aspnet:10.0
WORKDIR /app
COPY --from=build /app .
ENV ASPNETCORE_ENVIRONMENT=Production \
    ASPNETCORE_HTTP_PORTS=8080
EXPOSE 8080
USER $APP_UID
ENTRYPOINT ["dotnet", "Nexus.Table.Api.dll"]
