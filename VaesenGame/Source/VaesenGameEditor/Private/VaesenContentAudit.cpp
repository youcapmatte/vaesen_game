#include "VaesenContentAuditCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogVaesenContentAudit, Log, All);

namespace VaesenContentAudit
{
	/** The third-party packs (folders under /Game). Ours is /Game/Vaesen; the vendored copies go to /Game/Vaesen/ThirdParty. */
	const TCHAR* DefaultPacks[] = { TEXT("ThirdParty/Food_Pack_01"), TEXT("1HM_Sword_Attacks_Finishers"), TEXT("RifleAnims"), TEXT("WeaponVerse1"),
		TEXT("Combat_SFX_Bundle"), TEXT("Characters"), TEXT("ThirdPerson") };
	const TCHAR* VendorRoot = TEXT("/Game/Vaesen/ThirdParty");

	bool IsUnder(const FString& Package, const FString& Root)
	{
		return Package == Root || Package.StartsWith(Root + TEXT("/"));
	}

	int64 PackageSize(const FName Package)
	{
		FString File;
		if (!FPackageName::DoesPackageExist(Package.ToString(), &File))
		{
			return 0;
		}
		return IFileManager::Get().FileSize(*File);
	}

	/** The /Game paths the game's code names (it loads them by path; the registry can't see those). */
	TSet<FName> CodePaths()
	{
		TSet<FName> Paths;
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *(FPaths::GameSourceDir() / TEXT("VaesenGame")), TEXT("*.cpp"), true, false);
		IFileManager::Get().FindFilesRecursive(Files, *(FPaths::GameSourceDir() / TEXT("VaesenGame")), TEXT("*.h"), true, false);
		const FRegexPattern Pattern(TEXT("\"(/Game/[^\".]+)"));
		for (const FString& File : Files)
		{
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *File))
			{
				continue;
			}
			FRegexMatcher Matcher(Pattern, Text);
			while (Matcher.FindNext())
			{
				Paths.Add(FName(Matcher.GetCaptureGroup(1)));
			}
		}
		return Paths;
	}

	/** Where a pack's package goes when vendored: /Game/Vaesen/ThirdParty/<pack>/<the rest>. */
	FString VendoredPath(const FString& Package, const FString& PackRoot)
	{
		FString Pack = PackRoot.Mid(6);
		Pack.RemoveFromStart(TEXT("ThirdParty/"));
		return FString(VendorRoot) / Pack + Package.Mid(PackRoot.Len());
	}

	bool SavePackage(UPackage* Package)
	{
		UObject* Asset = Package->FindAssetInPackage();
		const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(),
			Package->ContainsMap() ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		const bool bSaved = UPackage::SavePackage(Package, Asset, *File, Args);
		if (!bSaved)
		{
			UE_LOG(LogVaesenContentAudit, Warning, TEXT("FAILED to save %s"), *Package->GetName());
		}
		return bSaved;
	}
}

UVaesenContentAuditCommandlet::UVaesenContentAuditCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UVaesenContentAuditCommandlet::Main(const FString& Params)
{
	using namespace VaesenContentAudit;
	const bool bMove = FParse::Param(*Params, TEXT("move"));
	TArray<FString> Packs;
	FString PackList;
	if (FParse::Value(*Params, TEXT("packs="), PackList))
	{
		PackList.ParseIntoArray(Packs, TEXT(","));
	}
	else
	{
		for (const TCHAR* Pack : DefaultPacks)
		{
			Packs.Add(Pack);
		}
	}
	TArray<FString> Roots;
	for (const FString& Pack : Packs)
	{
		Roots.Add(FString(TEXT("/Game/")) / Pack);
	}
	const auto PackOf = [&Roots](const FString& Package) -> int32
	{
		for (int32 Index = 0; Index < Roots.Num(); ++Index)
		{
			if (IsUnder(Package, Roots[Index]))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	};

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	Registry.SearchAllAssets(true);
	TArray<FAssetData> All;
	Registry.GetAssetsByPath(TEXT("/Game"), All, true);
	TSet<FName> Packages;
	for (const FAssetData& Asset : All)
	{
		Packages.Add(Asset.PackageName);
	}

	// Used: what our own content (everything outside the packs) and the code reach, then what that reaches, inside
	// any pack, until nothing new turns up.
	TMap<FName, FName> UsedBy;
	TArray<FName> Frontier;
	const auto Reach = [&](const FName From, const FName To)
	{
		if (PackOf(To.ToString()) != INDEX_NONE && !UsedBy.Contains(To) && Packages.Contains(To))
		{
			UsedBy.Add(To, From);
			Frontier.Add(To);
		}
	};
	for (const FName Package : Packages)
	{
		if (PackOf(Package.ToString()) != INDEX_NONE)
		{
			continue;
		}
		TArray<FName> Dependencies;
		Registry.GetDependencies(Package, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
		for (const FName Dependency : Dependencies)
		{
			Reach(Package, Dependency);
		}
	}
	for (const FName Path : CodePaths())
	{
		Reach(TEXT("(game code)"), Path);
	}
	while (Frontier.Num() > 0)
	{
		const FName Package = Frontier.Pop();
		TArray<FName> Dependencies;
		Registry.GetDependencies(Package, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
		for (const FName Dependency : Dependencies)
		{
			Reach(Package, Dependency);
		}
	}

	// -moveback=<pack>: what's in /Game/Vaesen/ThirdParty/<pack> that our content doesn't use goes back into the pack
	// (a move that took too much: everything a skeleton drags along). Where the pack still has its own copy, what used
	// the vendored one is pointed at the pack's and the vendored one deleted.
	FString MoveBack;
	if (FParse::Value(*Params, TEXT("moveback="), MoveBack))
	{
		const FString PackRoot = FString(TEXT("/Game/")) / MoveBack;
		const FString Vendored = VendoredPath(PackRoot, PackRoot);
		TSet<FName> Needed;
		TArray<FName> Open;
		for (const FName Package : Packages)
		{
			const FString Name = Package.ToString();
			if (IsUnder(Name, Vendored) || PackOf(Name) != INDEX_NONE || IsUnder(Name, PackRoot))
			{
				continue;
			}
			TArray<FName> Dependencies;
			Registry.GetDependencies(Package, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
			for (const FName Dependency : Dependencies)
			{
				if (IsUnder(Dependency.ToString(), Vendored) && !Needed.Contains(Dependency))
				{
					Needed.Add(Dependency);
					Open.Add(Dependency);
				}
			}
		}
		while (Open.Num() > 0)
		{
			TArray<FName> Dependencies;
			Registry.GetDependencies(Open.Pop(), Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
			for (const FName Dependency : Dependencies)
			{
				if (IsUnder(Dependency.ToString(), Vendored) && !Needed.Contains(Dependency))
				{
					Needed.Add(Dependency);
					Open.Add(Dependency);
				}
			}
		}
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		TArray<FAssetRenameData> Back;
		TArray<UObject*> Duplicates;
		for (const FName Package : Packages)
		{
			const FString Name = Package.ToString();
			if (!IsUnder(Name, Vendored) || Needed.Contains(Package))
			{
				continue;
			}
			const FString Original = PackRoot + Name.Mid(Vendored.Len());
			TArray<FAssetData> Assets;
			Registry.GetAssetsByPackageName(Package, Assets);
			for (const FAssetData& Asset : Assets)
			{
				// Redirectors left by an earlier pass are cleared below.
				UObject* Object = Asset.IsRedirector() ? nullptr : Asset.GetAsset();
				if (!Object)
				{
					continue;
				}
				if (FPackageName::DoesPackageExist(Original))
				{
					// The pack still has it: what used this copy uses the pack's.
					if (UObject* Theirs = LoadObject<UObject>(nullptr, *(Original + TEXT(".") + FPackageName::GetShortName(Original))))
					{
						TArray<UObject*> Replace = { Object };
						ObjectTools::ConsolidateObjects(Theirs, Replace, false);
						Duplicates.Add(Object);
					}
				}
				else
				{
					Back.Emplace(Object, FPackageName::GetLongPackagePath(Original), FPackageName::GetShortName(Original));
				}
			}
		}
		UE_LOG(LogVaesenContentAudit, Display, TEXT("Keeping %d packages in %s; moving %d assets back into %s; %d duplicates of the pack's own."),
			Needed.Num(), *Vendored, Back.Num(), *PackRoot, Duplicates.Num());
		if (Back.Num() > 0 && !AssetTools.RenameAssets(Back))
		{
			UE_LOG(LogVaesenContentAudit, Error, TEXT("Moving back failed."));
			return 1;
		}
		int32 Saved = 0;
		for (TObjectIterator<UPackage> It; It; ++It)
		{
			if (It->IsDirty() && It->GetName().StartsWith(TEXT("/Game/")) && It->FindAssetInPackage())
			{
				Saved += SavePackage(*It) ? 1 : 0;
			}
		}
		UE_LOG(LogVaesenContentAudit, Display, TEXT("Saved %d packages."), Saved);

		// What's left of the unused vendored copies are redirectors to the pack's own. Pack assets that still point
		// through them are loaded (which follows them) and saved (which writes the pack's own place); then they go.
		Registry.SearchAllAssets(true);
		TArray<FAssetData> Now;
		Registry.GetAssetsByPath(FName(*Vendored), Now, true);
		TSet<FName> Redirectors;
		for (const FAssetData& Asset : Now)
		{
			if (Asset.IsRedirector() && !Needed.Contains(Asset.PackageName))
			{
				Redirectors.Add(Asset.PackageName);
			}
		}
		TArray<FAssetData> InPack;
		Registry.GetAssetsByPath(FName(*PackRoot), InPack, true);
		TSet<FName> Resave;
		for (const FAssetData& Asset : InPack)
		{
			TArray<FName> Dependencies;
			Registry.GetDependencies(Asset.PackageName, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
			if (Dependencies.ContainsByPredicate([&Redirectors](const FName Dependency) { return Redirectors.Contains(Dependency); }))
			{
				Resave.Add(Asset.PackageName);
			}
		}
		int32 Fixed = 0;
		for (const FName Package : Resave)
		{
			if (UPackage* Loaded = LoadPackage(nullptr, *Package.ToString(), LOAD_None))
			{
				Loaded->MarkPackageDirty();
				Fixed += SavePackage(Loaded) ? 1 : 0;
			}
		}
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		int32 Deleted = 0;
		for (const FName Package : Redirectors)
		{
			FString File;
			if (FPackageName::DoesPackageExist(Package.ToString(), &File) && IFileManager::Get().Delete(*File, false, true))
			{
				++Deleted;
			}
		}
		UE_LOG(LogVaesenContentAudit, Display, TEXT("Re-saved %d pack packages past the vendored redirectors; deleted %d redirectors."), Fixed, Deleted);
		return 0;
	}

	// The report: each pack's size, what of it is used and by what.
	for (int32 Index = 0; Index < Roots.Num(); ++Index)
	{
		int64 Total = 0, UsedSize = 0;
		int32 Count = 0;
		TArray<FName> Used;
		for (const FName Package : Packages)
		{
			if (PackOf(Package.ToString()) != Index)
			{
				continue;
			}
			++Count;
			const int64 Size = PackageSize(Package);
			Total += Size;
			if (UsedBy.Contains(Package))
			{
				Used.Add(Package);
				UsedSize += Size;
			}
		}
		Used.Sort([](const FName A, const FName B) { return A.LexicalLess(B); });
		UE_LOG(LogVaesenContentAudit, Display, TEXT("=== %s: %d assets, %.1f MB; used: %d, %.1f MB"), *Roots[Index], Count, Total / 1048576.0, Used.Num(), UsedSize / 1048576.0);
		for (const FName Package : Used)
		{
			UE_LOG(LogVaesenContentAudit, Display, TEXT("    %-90s %7.2f MB  <- %s"), *Package.ToString(), PackageSize(Package) / 1048576.0, *UsedBy[Package].ToString());
		}
	}
	// Repair: our assets that still point at a moved asset's old place (a redirector left in a pack) are loaded (which
	// follows the redirector) and saved (which writes the new place).
	if (FParse::Param(*Params, TEXT("fixrefs")))
	{
		int32 Fixed = 0;
		for (const FName Package : Packages)
		{
			if (PackOf(Package.ToString()) != INDEX_NONE)
			{
				continue;
			}
			TArray<FName> Dependencies;
			Registry.GetDependencies(Package, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
			const bool bThroughRedirector = Dependencies.ContainsByPredicate([&](const FName Dependency)
			{
				TArray<FAssetData> Assets;
				Registry.GetAssetsByPackageName(Dependency, Assets, true);
				return PackOf(Dependency.ToString()) != INDEX_NONE && Assets.ContainsByPredicate([](const FAssetData& Asset) { return Asset.IsRedirector(); });
			});
			if (bThroughRedirector)
			{
				if (UPackage* Loaded = LoadPackage(nullptr, *Package.ToString(), LOAD_None))
				{
					Loaded->MarkPackageDirty();
					UE_LOG(LogVaesenContentAudit, Display, TEXT("Re-saving %s (it pointed at a moved asset's old place)"), *Package.ToString());
					Fixed += SavePackage(Loaded) ? 1 : 0;
				}
			}
		}
		UE_LOG(LogVaesenContentAudit, Display, TEXT("Re-saved %d packages."), Fixed);
		return 0;
	}
	if (!bMove)
	{
		return 0;
	}

	// Move what's used into /Game/Vaesen/ThirdParty/<pack>/..., references and all.
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	TArray<FAssetRenameData> Renames;

	for (const TPair<FName, FName>& Entry : UsedBy)
	{
		const FString Package = Entry.Key.ToString();
		const int32 Index = PackOf(Package);
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPackageName(Entry.Key, Assets);
		for (const FAssetData& Asset : Assets)
		{
			if (UObject* Object = Asset.GetAsset())
			{
				const FString NewPackage = VendoredPath(Package, Roots[Index]);
				Renames.Emplace(Object, FPackageName::GetLongPackagePath(NewPackage), FPackageName::GetShortName(NewPackage));
			}
		}
	}
	UE_LOG(LogVaesenContentAudit, Display, TEXT("Moving %d assets into %s..."), Renames.Num(), VendorRoot);
	if (!AssetTools.RenameAssets(Renames))
	{
		UE_LOG(LogVaesenContentAudit, Error, TEXT("Moving failed."));
		return 1;
	}

	// The rename loads what refers to the moved assets, points it at the new places and saves it. A redirector is
	// left in the pack only where something else in the pack (unused) still refers to the old place; the pack is
	// ignored, so it stays there. (Fixing up redirectors opens a report window, which a commandlet hasn't got.)

	// Anything else it changed, saved.
	int32 Saved = 0;
	for (TObjectIterator<UPackage> It; It; ++It)
	{
		if (It->IsDirty() && It->GetName().StartsWith(TEXT("/Game/")) && It->FindAssetInPackage())
		{
			Saved += SavePackage(*It) ? 1 : 0;
		}
	}
	UE_LOG(LogVaesenContentAudit, Display, TEXT("Moved %d assets; saved %d packages."), Renames.Num(), Saved);
	return 0;
}
