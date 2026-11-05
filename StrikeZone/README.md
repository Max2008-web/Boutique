# STRIKE ZONE — FPS 3D multiplateforme (Unreal Engine 5.6, C++)

Projet Unreal Engine 5.6 en C++ natif : un FPS 3D jouable sur Windows et Android, avec six modes de jeu, des bots IA, des zombies et un multijoueur à serveur autoritaire.

> **État réel du projet.** Le code n'a **pas été compilé ni testé** : aucun Unreal Engine n'était disponible dans l'environnement où il a été écrit. Il a seulement été relu et vérifié statiquement (includes, déclarations/définitions, signatures des délégués et RPC). Il faut s'attendre à quelques erreurs de compilation mineures au premier build. Le chapitre « Dépannage » liste les points à surveiller.

Le jeu fonctionne **sans aucun asset** : géométrie, armes, personnages et interface utilisent les formes de base du moteur et le Canvas. Les emplacements d'assets (meshes, Animation Blueprints, Niagara, sons, Behavior Trees, EQS) sont prévus pour remplacer ces éléments temporaires.

---

## 1. Arborescence

```
StrikeZone/
├── StrikeZone.uproject
├── Config/
│   ├── DefaultEngine.ini          cartes, alias de modes, canal "Weapon", rendu PC, Android, NavMesh dynamique
│   ├── DefaultGame.ini            packaging, MaxPlayers, réglages des modes
│   ├── DefaultInput.ini           Enhanced Input, joystick moteur désactivé
│   ├── DefaultDeviceProfiles.ini  profils Android_Low / Mid / High
│   └── Android/AndroidEngine.ini  pas de Lumen/VSM/Distance Fields sur mobile
└── Source/
    ├── StrikeZone.Target.cs / StrikeZoneEditor.Target.cs / StrikeZoneServer.Target.cs
    └── StrikeZone/
        ├── StrikeZone.Build.cs, StrikeZone.h/.cpp        module
        ├── Core/        StrikeTypes (enums, structs, interface IStrikeInteractable),
        │                StrikeSaveGame (réglages + coffre), StrikeGameInstance (profil, graphismes)
        ├── Character/   AStrikeCharacter, UStrikeCharacterMovement (sprint/glissade prédits),
        │                UStrikeAnimInstance (base des Animation Blueprints)
        ├── Components/  UHealthComponent, UWeaponComponent, UInventoryComponent
        ├── Weapons/     AStrikeWeapon (base), armes concrètes, AStrikeProjectile, AStrikeTracer
        ├── Game/        AStrikeGameMode (base), AStrikeGameState, AStrikePlayerState, AStrikePlayerController
        ├── Modes/       Menu, Zombies, Partie rapide, Battle Royale, Tactique, Extraction, Entraînement
        ├── AI/          AStrikeAIController (soldats), AStrikeZombie + IA de horde, nœuds de Behavior Tree
        ├── World/       AStrikeArenaBuilder (cartes procédurales), points d'apparition, ramassages,
        │                achats muraux, zones d'objectif, cibles, zone du Battle Royale
        └── UI/          AStrikeHUD (combat, scoreboard, menu, réglages), UStrikeTouchControls
```

Dépendances déclarées (`StrikeZone.Build.cs`) : `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `AIModule`, `GameplayTasks`, `NavigationSystem`, `Niagara` (public), `Slate`, `SlateCore` (privé). Plugins : `EnhancedInput`, `Niagara`.

## 2. Ce qui est implémenté

| Système | Où | Détails |
|---|---|---|
| Déplacement FPS | `StrikeCharacter`, `StrikeCharacterMovement` | Sprint et glissade dans la prédiction réseau (`FSavedMove` + drapeaux compressés), saut, accroupissement maintenu ou bascule, caméra dont la hauteur suit l'accroupissement |
| Visée ADS | `StrikeCharacter::Tick`, `StrikeWeapon::UpdateWeaponPose` | Transition du FOV et de la position de l'arme, lunette pour le fusil de précision, sensibilité réduite en visée |
| Tir hitscan / projectile | `StrikeWeapon`, `StrikeProjectile` | Plombs du fusil à pompe, roquettes à dégâts de zone (Chaos), traceurs |
| Recul, dispersion | `StrikeWeapon` | Cône selon la visée, le mouvement, la chute ou l'accroupissement ; dispersion cumulative par tir ; recul avec récupération ; réticule dynamique |
| Rechargement, munitions | `StrikeWeapon`, `InventoryComponent` | Chargeur côté serveur, réserves par calibre, munitions prédites chez le client sans saut visuel |
| Dégâts localisés | `HealthComponent::GetHitZoneFromHit` | Par os (Skeletal Mesh), par tag `Head` ou par hauteur sur la capsule ; multiplicateurs propres à chaque arme ; atténuation avec la distance |
| Santé, armure | `HealthComponent` | Absorption par l'armure, régénération réglable par mode, indicateur de direction des dégâts |
| Mort, réapparition | `StrikeCharacter::HandleDeath`, `StrikeGameMode` | Chute physique (ragdoll Chaos si un Skeletal Mesh est fourni), fil des éliminations, réapparition différée ou mode spectateur |
| Réseau | Partout | Serveur autoritaire, tirs validés par le serveur (voir §6), réplication des personnages, armes, scores et objectifs |
| IA soldats | `StrikeAIController` | Perception visuelle et auditive (bruit des tirs), poursuite, couverture (EQS optionnel ou recherche native), repli, rafales, 4 difficultés, Behavior Tree optionnel |
| IA zombies | `StrikeZombie` | IA de horde distincte des soldats, 4 types (marcheur, coureur, brute, boss), corps à corps |
| Commandes PC / manette | `StrikePlayerController` | Enhanced Input créé en C++ (aucun asset nécessaire) |
| Commandes tactiles | `StrikeTouchControls`, `StrikeHUD` | Joystick dynamique à gauche, caméra tactile à droite, 10 boutons, sprint en poussant le joystick à fond vers l'avant, interface proportionnelle à la hauteur de l'écran |
| Réglages | `StrikeSaveGame`, menu | Sensibilité souris, manette et tactile ; inversion Y ; FOV ; qualité graphique ; résolution dynamique ; difficulté des bots |
| Sauvegarde | `StrikeGameInstance` | Réglages, coffre persistant de l'Extraction, record de vagues des Zombies |

### Modes (chacun a son GameMode C++ et réutilise les composants communs)

| Alias URL | Classe | Contenu |
|---|---|---|
| `Menu` | `AStrikeGameMode_Menu` | Menu principal, choix du mode, option « héberger », réglages |
| `Zombies` | `AStrikeGameMode_Zombies` | Vagues progressives, coureurs dès la vague 2, brutes dès la vague 3, boss toutes les 5 vagues, points par touche et par élimination, 5 achats muraux, caisses de munitions et d'armure, réapparition en fin de vague, coopération |
| `QuickMatch` | `AStrikeGameMode_TeamDeathmatch` | Match à mort par équipes 6v6 complété par des bots, limite de score à 50, 10 min, réapparition en 3 s, armes au centre |
| `BattleRoyale` | `AStrikeGameMode_BattleRoyale` | Carte de 300 m, jusqu'à 30 combattants, 60 points de butin, 5 phases de zone avec dégâts croissants, mur de zone visible, bots guidés vers la zone |
| `Tactical` | `AStrikeGameMode_Tactical` | 5v5, premier à 7 manches, changement de camp à la mi-temps, pas de réapparition pendant la manche, gel en début de manche, pose et désamorçage d'une charge sur les sites A et B, bots qui jouent l'objectif |
| `Extraction` | `AStrikeGameMode_Extraction` | Exploration en coopération, butin avec rareté, 18 mercenaires IA avec renforts, 3 zones d'extraction ouvertes après 90 s, 8 s dans la zone pour s'extraire, butin ajouté au coffre persistant |
| `Training` | `AStrikeGameMode_Training` | Stand de tir de 75 m, cibles fixes et mobiles avec tête, râteliers des 6 armes, bots passifs, munitions infinies, statistiques de précision et de tirs à la tête |

Paramètres d'URL communs : `?Bots=12?Difficulty=0..3?Time=600?Score=50?listen`.

## 3. Intégration et compilation

### Prérequis
- **Unreal Engine 5.6** installé depuis l'Epic Games Launcher (ou une version plus récente compatible).
- **Windows** : Visual Studio 2022 avec les workloads « Développement Desktop C++ » et « Développement de jeux en C++ ».
- **Android** : Android Studio avec le SDK et le NDK aux versions exigées par UE 5.6 (voir la page *Android Development Requirements* de la documentation Epic, ou configurez-les avec Turnkey : *Platforms > Android > Installed SDK*).

### Étape 1 : générer et compiler
1. Clic droit sur `StrikeZone/StrikeZone.uproject` > **Generate Visual Studio project files**.
2. Ouvrez `StrikeZone.sln` et compilez en **Development Editor | Win64**.
   En ligne de commande :
   ```
   "<UE_5.6>\Engine\Build\BatchFiles\Build.bat" StrikeZoneEditor Win64 Development -Project="<chemin>\StrikeZone.uproject" -WaitMutex
   ```
3. Ouvrez le `.uproject`.

### Étape 2 : créer la carte (seul asset nécessaire)
1. **File > New Level > Empty Level** (ou *Basic*).
2. Enregistrez-la sous `/Game/Maps/StrikeArena` (nom attendu par `DefaultEngine.ini`).
3. **NavMesh, recommandé** : ajoutez un `NavMeshBoundsVolume` centré en (0, 0, 0) avec une échelle de **(160, 160, 10)**. Il couvre les trois dispositions de carte. La génération est dynamique (`RuntimeGeneration=Dynamic`) car l'arène est construite au lancement. Sans NavMesh, l'IA utilise un pilotage direct avec un évitement simple : le jeu reste jouable, mais la navigation est moins bonne.

L'arène, l'éclairage, les points d'apparition et les objets de jeu sont générés au lancement par `AStrikeArenaBuilder`. Si vous créez un niveau à la main, placez-y des `AStrikeSpawnPoint` : le builder automatique est alors désactivé.

### Étape 3 : tester
- **Solo** : lancez *Play* : le menu s'affiche. Cliquez sur un mode.
- **Console** : `open StrikeArena?game=Zombies`, `open StrikeArena?game=QuickMatch?Bots=12?Difficulty=2`.
- **Multijoueur dans l'éditeur** : *Play > Advanced Settings* > *Number of Players* = 2, *Net Mode* = **Play As Listen Server**, puis lancez la carte avec `?game=QuickMatch` (par exemple en mettant *GameMode Override* sur `StrikeGameMode_TeamDeathmatch` dans les *World Settings*).
- **Réseau local** : l'hôte lance `open StrikeArena?game=QuickMatch?listen` (ou coche « Héberger » dans le menu) ; le client tape `open <IP de l'hôte>`.
- **Serveur dédié** : la cible `StrikeZoneServer` nécessite une version d'Unreal Engine compilée depuis les sources (GitHub Epic).

Commandes console : `StrikeResetStats`, `StrikeSensitivity <souris> <tactile>`, `StrikeQuality <-1..3>`, `StrikeTouch 1` (forcer le tactile sur PC).

### Étape 4 : builds Windows et Android
- **Windows** : *Platforms > Windows > Package Project*, ou :
  ```
  RunUAT.bat BuildCookRun -project="<chemin>\StrikeZone.uproject" -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive -archivedirectory="<sortie>"
  ```
- **Android** : *Platforms > Android > Package Project* (format de textures **ASTC**), ou :
  ```
  RunUAT.bat BuildCookRun -project="<chemin>\StrikeZone.uproject" -platform=Android -cookflavor=ASTC -clientconfig=Development -build -cook -stage -pak -package -archive -archivedirectory="<sortie>"
  ```
  Installez avec le script `Install_StrikeZone-arm64.bat` généré dans le dossier de sortie, l'appareil étant branché en débogage USB.

## 4. Commandes

| Action | Clavier / souris | Manette | Tactile |
|---|---|---|---|
| Se déplacer | WASD (positions physiques : ZQSD sur AZERTY) | Stick gauche | Joystick à gauche (dynamique) |
| Regarder | Souris | Stick droit | Glisser à droite (ou sur FEU / VISER) |
| Tirer | Clic gauche | RT | FEU (deux boutons) |
| Viser (ADS) | Clic droit maintenu | LT | VISER (bascule) |
| Sauter | Espace | A | SAUT |
| S'accroupir / glisser | Ctrl ou C (en sprintant = glissade) | B | ACCR. (bascule) |
| Sprinter | Shift maintenu | Clic du stick gauche | Joystick poussé à fond vers l'avant |
| Recharger | R | X | RECH. |
| Utiliser / acheter / poser | E ou F (maintenir pour poser ou désamorcer) | Croix directionnelle haut | UTIL. |
| Changer d'arme | 1/2/3, molette | Y | ARME |
| Tableau des scores | Tab | View | SCORE |
| Retour au menu | Échap ou P | Menu | II |

## 5. Plateformes et performances
- **PC** : Lumen, ombres virtuelles, TSR, DirectX 12. Qualité Bas, Moyen, Élevé ou Épique dans le menu (`UGameUserSettings`).
- **Android** : Lumen, VSM et Distance Fields sont désactivés par `Config/Android/AndroidEngine.ini`. Profils `Android_Low/Mid/High` : qualité des ombres, des effets et des textures, et `r.MobileContentScaleFactor`. Vulkan, avec repli sur OpenGL ES.
- **Résolution dynamique** : activée via `UGameUserSettings`. Sur mobile, un ajustement de secours de `r.ScreenPercentage` (50 à 100 %) selon les FPS mesurés est intégré au contrôleur.
- Plafond de zombies simultanés (24), apparition progressive, tick réduit de la santé (10 Hz), lumières de bouche sans ombres.
- **iOS** : aucune API spécifique à une plateforme n'est utilisée hors des blocs `PLATFORM_ANDROID || PLATFORM_IOS`. Ajoutez `IOS` dans `TargetPlatforms` et configurez la signature.

## 6. Sécurité réseau
- Le client n'annonce **jamais** de touche. Il envoie `ServerFireShot(origine, direction, graine)`. Le serveur refait lui-même les traces et applique les dégâts.
- Le serveur vérifie :
  - la cadence, avec un seau de jetons qui tolère la gigue réseau mais bloque une cadence trop rapide ;
  - les munitions, le rechargement et l'état équipé ;
  - l'origine du tir, qui doit être proche de l'œil connu du serveur (sinon il la corrige) ;
  - la direction, qui doit rester à moins de 40° de la rotation de contrôle côté serveur.
- La dispersion utilise une graine partagée : client et serveur obtiennent le même cône.
- La santé, l'armure, les points, les achats, les ramassages, les objectifs et l'extraction sont tous décidés par le serveur.
- Déconnexions : un bot reprend la place libérée et les statistiques sont conservées 2 min en cas de reconnexion (`InactivePlayerStateLifeSpan`, `CopyProperties`). En Tactique et en Battle Royale, un joueur parti compte comme éliminé.

## 7. Remplacer les éléments temporaires
- **Personnage** : créez un Blueprint enfant de `AStrikeCharacter`. Assignez un Skeletal Mesh à `Mesh` et un Animation Blueprint dont le parent est `UStrikeAnimInstance` : les variables `Speed`, `Direction`, `AimPitch`, `bIsCrouching`, `bIsSliding`, etc. alimentent la State Machine, l'Aim Offset et l'IK. Masquez `BodyMesh` et `HeadMesh`.
- **Armes** : créez des Blueprints enfants des armes C++. Remplacez le mesh du composant `Mesh` et renseignez `FireSound`, `MuzzleFX`, `ImpactFX` (Niagara), `FireMontage` et `ReloadMontage`. Toutes les statistiques sont modifiables.
- **IA** : assignez `BehaviorTreeAsset`, qui utilise les nœuds `Strike : Mise à jour combat`, `Strike : Tirer sur la cible` et `Strike : Trouver une couverture` avec les clés `TargetActor`, `HasLineOfSight`, `LastKnownLocation` et `HealthFraction`. Assignez aussi `CoverQuery` (EQS) dans un Blueprint enfant de `AStrikeAIController`.
- **Interface** : `AStrikeHUD` (Canvas) peut être remplacé écran par écran par des widgets UMG / Common UI branchés sur les mêmes données (GameState, PlayerState, composants).

## 8. Limites connues et prochaines étapes
- **Pas encore compilé.** Points sensibles à vérifier au premier build :
  - le constructeur `FCanvasNGonItem(Position, Rayon, Côtés, Couleur)` ;
  - `UAISense::GetSenseID<>()` ;
  - les setters `SetNetUpdateFrequency` et `SetNetCullDistanceSquared` (UE 5.5 et plus) ;
  - le paramètre `Color` du matériau `BasicShapeMaterial` : s'il n'existe pas, les formes restent grises sans erreur.
- **Gameplay Ability System** : pas encore utilisé. Les composants actuels couvrent santé, armes et munitions sans dépendance supplémentaire. GAS est prévu pour les compétences (grenades, atouts, soins), qui y gagnent vraiment.
- **UMG / Common UI** : l'interface actuelle est dessinée au Canvas pour fonctionner sans asset. La migration est à faire.
- **Compensation de latence** : le serveur effectue les traces à l'instant présent (pas de retour en arrière des hitbox). À ajouter pour les joueurs à ping élevé.
- **Sessions en ligne** : connexion par IP ou réseau local uniquement. L'étape suivante est OnlineSubsystem / EOS pour le matchmaking.
- **Inventaire persistant** : le coffre de l'Extraction est sauvegardé localement (SaveGame). En ligne, il faudra un service backend autoritaire.
- **Anti-triche** : validation serveur uniquement. Aucune protection côté client (EAC ou équivalent) n'est intégrée.

## 9. Plan de développement

| Étape | Contenu | État |
|---|---|---|
| 1 | Projet C++, personnage FPS, caméra, déplacements, commandes PC et mobile | Code écrit |
| 2 | Armes : tir, rechargement, dégâts | Code écrit |
| 3 | IA et mode Zombies jouable | Code écrit |
| 4 | Partie rapide avec bots, score, chronomètre | Code écrit |
| 5 | Multijoueur et autres modes | Code écrit (Listen Server ; serveur dédié avec moteur compilé depuis les sources) |
| 6 | Optimisation, tests, builds Windows et Android | Configurations prêtes ; **compilation et tests sur machine à faire** |
