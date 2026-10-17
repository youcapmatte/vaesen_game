#include "Core/VaesenSettings.h"

UVaesenSettings::UVaesenSettings()
{
	ApiBaseUrl = TEXT("http://localhost:5230");
	LoginMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Vaesen/Maps/L_Login.L_Login")));
	CharacterSelectMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Vaesen/Maps/L_CharacterSelect.L_CharacterSelect")));
	GuildHubMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Vaesen/Maps/L_Gyllencreutz.L_Gyllencreutz")));

	LoadingTips = {
		NSLOCTEXT("VaesenTips", "Tip1", "Vaesen are bound by old rules. Learn a creature's weakness before you face it."),
		NSLOCTEXT("VaesenTips", "Tip2", "Only those who have seen the hidden world can see the vaesen. Most people never will."),
		NSLOCTEXT("VaesenTips", "Tip3", "Iron, salt and church bells have kept the vaesen at bay for centuries."),
		NSLOCTEXT("VaesenTips", "Tip4", "Gyllencreutz is your guild's home. Rest, plan and prepare there between investigations."),
		NSLOCTEXT("VaesenTips", "Tip5", "Talk to the locals. Every vaesen leaves traces in the stories people tell."),
		NSLOCTEXT("VaesenTips", "Tip6", "Fear is a wound too. Return to Gyllencreutz to recover."),
	};

	// 19th-century Scandinavian names.
	RandomFirstNames = {
		TEXT("Agnes"), TEXT("Albin"), TEXT("Alma"), TEXT("Anna"), TEXT("Arvid"), TEXT("Astrid"), TEXT("Axel"), TEXT("Beda"),
		TEXT("Birger"), TEXT("Britta"), TEXT("Carl"), TEXT("Dagmar"), TEXT("Ebba"), TEXT("Edvard"), TEXT("Elin"), TEXT("Emil"),
		TEXT("Erik"), TEXT("Esther"), TEXT("Frida"), TEXT("Gunnar"), TEXT("Gustav"), TEXT("Hedvig"), TEXT("Helga"), TEXT("Henrik"),
		TEXT("Hilda"), TEXT("Ingrid"), TEXT("Ivar"), TEXT("Johan"), TEXT("Karin"), TEXT("Knut"), TEXT("Lars"), TEXT("Magnus"),
		TEXT("Maja"), TEXT("Margit"), TEXT("Nils"), TEXT("Olof"), TEXT("Oscar"), TEXT("Ragnhild"), TEXT("Sigrid"), TEXT("Sven"),
		TEXT("Tora"), TEXT("Ulrika"), TEXT("Vilhelm"), TEXT("Ylva"),
	};
	RandomLastNames = {
		TEXT("Almqvist"), TEXT("Berg"), TEXT("Bergström"), TEXT("Björk"), TEXT("Dahl"), TEXT("Ekberg"), TEXT("Ekström"), TEXT("Falk"),
		TEXT("Forsberg"), TEXT("Gyllenstierna"), TEXT("Hallberg"), TEXT("Holm"), TEXT("Hägg"), TEXT("Lagerlöf"), TEXT("Lind"),
		TEXT("Lindqvist"), TEXT("Lund"), TEXT("Lönnrot"), TEXT("Nordin"), TEXT("Nyström"), TEXT("Rosén"), TEXT("Sandberg"),
		TEXT("Sjöberg"), TEXT("Strand"), TEXT("Strindberg"), TEXT("Söderlund"), TEXT("Wahlberg"), TEXT("Wallin"), TEXT("Åkerman"),
		TEXT("Östlund"),
	};
}
