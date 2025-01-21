project(application)

################################ FILES ################################

######## Sources and headers ########

set(CORE_SOURCES
    # LOGIC - Base classes and infrastructure
    BaseInstaller.h
    BaseInstaller.cpp
    BaseVersionList.h
    BaseVersionList.cpp
    InstanceList.h
    InstanceList.cpp
    InstanceTask.h
    InstanceTask.cpp
    LoggedProcess.h
    LoggedProcess.cpp
    MessageLevel.cpp
    MessageLevel.h
    BaseVersion.h
    BaseInstance.h
    BaseInstance.cpp
    NullInstance.h
    MMCZip.h
    MMCZip.cpp
    Untar.h
    Untar.cpp
    StringUtils.h
    StringUtils.cpp
    QVariantUtils.h
    RuntimeContext.h
    PSaveFile.h

    # Basic instance manipulation tasks (derived from InstanceTask)
    InstanceCreationTask.h
    InstanceCreationTask.cpp
    InstanceCopyPrefs.h
    InstanceCopyPrefs.cpp
    InstanceCopyTask.h
    InstanceCopyTask.cpp
    InstanceImportTask.h
    InstanceImportTask.cpp

    # Resource downloading task
    ResourceDownloadTask.h
    ResourceDownloadTask.cpp

    # Use tracking separate from memory management
    Usable.h

    # Prefix tree where node names are strings between separators
    SeparatorPrefixTree.h

    # String filters
    Filter.h
    Filter.cpp

    # JSON parsing helpers
    Json.h
    Json.cpp

    FileSystem.h
    FileSystem.cpp

    Exception.h

    # RW lock protected map
    RWStorage.h

    # A variable that has an implicit default value and keeps track of changes
    DefaultVariable.h

    # a smart pointer wrapper intended for safer use with Qt signal/slot mechanisms
    QObjectPtr.h

    # Compression support
    GZip.h
    GZip.cpp

    # Command line parameter parsing
    Commandline.h
    Commandline.cpp

    # Version number string support
    Version.h
    Version.cpp

    # A Recursive file system watcher
    RecursiveFileSystemWatcher.h
    RecursiveFileSystemWatcher.cpp

    # Time
    MMCTime.h
    MMCTime.cpp

    MTPixmapCache.h
)
if (UNIX AND NOT CYGWIN AND NOT APPLE)
set(CORE_SOURCES
    ${CORE_SOURCES}

    # MangoHud
    MangoHud.h
    MangoHud.cpp
    )
endif()

set(PATHMATCHER_SOURCES
    # Path matchers
    pathmatcher/FSTreeMatcher.h
    pathmatcher/IPathMatcher.h
    pathmatcher/MultiMatcher.h
    pathmatcher/RegexpMatcher.h
    pathmatcher/SimplePrefixMatcher.h
)

set(NET_SOURCES
    # network stuffs
    net/ByteArraySink.h
    net/ChecksumValidator.h
    net/Download.cpp
    net/Download.h
    net/FileSink.cpp
    net/FileSink.h
    net/HttpMetaCache.cpp
    net/HttpMetaCache.h
    net/MetaCacheSink.cpp
    net/MetaCacheSink.h
    net/Logging.h
    net/Logging.cpp
    net/NetJob.cpp
    net/NetJob.h
    net/NetUtils.h
    net/PasteUpload.cpp
    net/PasteUpload.h
    net/Sink.h
    net/Validator.h
    net/Upload.cpp
    net/Upload.h
    net/HeaderProxy.h
    net/RawHeaderProxy.h
    net/ApiHeaderProxy.h
    net/ApiDownload.h
    net/ApiDownload.cpp
    net/ApiUpload.cpp
    net/ApiUpload.h
    net/NetRequest.cpp
    net/NetRequest.h
)

# Game launch logic
set(LAUNCH_SOURCES
    launch/steps/CheckJava.cpp
    launch/steps/CheckJava.h
    launch/steps/LookupServerAddress.cpp
    launch/steps/LookupServerAddress.h
    launch/steps/PostLaunchCommand.cpp
    launch/steps/PostLaunchCommand.h
    launch/steps/PreLaunchCommand.cpp
    launch/steps/PreLaunchCommand.h
    launch/steps/TextPrint.cpp
    launch/steps/TextPrint.h
    launch/steps/QuitAfterGameStop.cpp
    launch/steps/QuitAfterGameStop.h
    launch/steps/PrintServers.cpp
    launch/steps/PrintServers.h
    launch/LaunchStep.cpp
    launch/LaunchStep.h
    launch/LaunchTask.cpp
    launch/LaunchTask.h
    launch/LogModel.cpp
    launch/LogModel.h
    launch/TaskStepWrapper.cpp
    launch/TaskStepWrapper.h
)

# Old update system
set(UPDATE_SOURCES
    updater/ExternalUpdater.h
)

set(MAC_UPDATE_SOURCES
    updater/MacSparkleUpdater.h
    updater/MacSparkleUpdater.mm
)

set(PRISM_UPDATE_SOURCES
    updater/PrismExternalUpdater.h
    updater/PrismExternalUpdater.cpp
)

# Backend for the news bar... there's usually no news.
set(NEWS_SOURCES
    # News System
    news/NewsChecker.h
    news/NewsChecker.cpp
    news/NewsEntry.h
    news/NewsEntry.cpp
)

# Icon interface
set(ICONS_SOURCES
    # Icons System and related code
    icons/IconUtils.h
    icons/IconUtils.cpp
)

# Support for Minecraft instances and launch
set(MINECRAFT_SOURCES

    # Logging
    minecraft/Logging.h
    minecraft/Logging.cpp

    # Minecraft support
    minecraft/auth/AccountData.cpp
    minecraft/auth/AccountData.h
    minecraft/auth/AccountList.cpp
    minecraft/auth/AccountList.h
    minecraft/auth/AuthSession.cpp
    minecraft/auth/AuthSession.h
    minecraft/auth/AuthStep.h
    minecraft/auth/MinecraftAccount.cpp
    minecraft/auth/MinecraftAccount.h
    minecraft/auth/Parsers.cpp
    minecraft/auth/Parsers.h

    minecraft/auth/AuthFlow.cpp
    minecraft/auth/AuthFlow.h

    minecraft/auth/steps/EntitlementsStep.cpp
    minecraft/auth/steps/EntitlementsStep.h
    minecraft/auth/steps/GetSkinStep.cpp
    minecraft/auth/steps/GetSkinStep.h
    minecraft/auth/steps/LauncherLoginStep.cpp
    minecraft/auth/steps/LauncherLoginStep.h
    minecraft/auth/steps/MinecraftProfileStep.cpp
    minecraft/auth/steps/MinecraftProfileStep.h
    minecraft/auth/steps/MSADeviceCodeStep.cpp
    minecraft/auth/steps/MSADeviceCodeStep.h
    minecraft/auth/steps/MSAStep.cpp
    minecraft/auth/steps/MSAStep.h
    minecraft/auth/steps/XboxAuthorizationStep.cpp
    minecraft/auth/steps/XboxAuthorizationStep.h
    minecraft/auth/steps/XboxProfileStep.cpp
    minecraft/auth/steps/XboxProfileStep.h
    minecraft/auth/steps/XboxUserStep.cpp
    minecraft/auth/steps/XboxUserStep.h

    minecraft/gameoptions/GameOptions.h
    minecraft/gameoptions/GameOptions.cpp

    minecraft/update/AssetUpdateTask.h
    minecraft/update/AssetUpdateTask.cpp
    minecraft/update/FMLLibrariesTask.cpp
    minecraft/update/FMLLibrariesTask.h
    minecraft/update/FoldersTask.cpp
    minecraft/update/FoldersTask.h
    minecraft/update/LibrariesTask.cpp
    minecraft/update/LibrariesTask.h

    minecraft/launch/ClaimAccount.cpp
    minecraft/launch/ClaimAccount.h
    minecraft/launch/CreateGameFolders.cpp
    minecraft/launch/CreateGameFolders.h
    minecraft/launch/ModMinecraftJar.cpp
    minecraft/launch/ModMinecraftJar.h
    minecraft/launch/ExtractNatives.cpp
    minecraft/launch/ExtractNatives.h
    minecraft/launch/LauncherPartLaunch.cpp
    minecraft/launch/LauncherPartLaunch.h
    minecraft/launch/MinecraftTarget.cpp
    minecraft/launch/MinecraftTarget.h
    minecraft/launch/PrintInstanceInfo.cpp
    minecraft/launch/PrintInstanceInfo.h
    minecraft/launch/ReconstructAssets.cpp
    minecraft/launch/ReconstructAssets.h
    minecraft/launch/ScanModFolders.cpp
    minecraft/launch/ScanModFolders.h
    minecraft/launch/VerifyJavaInstall.cpp
    minecraft/launch/VerifyJavaInstall.h
    minecraft/launch/AutoInstallJava.cpp
    minecraft/launch/AutoInstallJava.h

    minecraft/GradleSpecifier.h
    minecraft/MinecraftInstance.cpp
    minecraft/MinecraftInstance.h
    minecraft/LaunchProfile.cpp
    minecraft/LaunchProfile.h
    minecraft/Component.cpp
    minecraft/Component.h
    minecraft/PackProfile.cpp
    minecraft/PackProfile.h
    minecraft/ComponentUpdateTask.cpp
    minecraft/ComponentUpdateTask.h
    minecraft/MinecraftLoadAndCheck.h
    minecraft/MinecraftLoadAndCheck.cpp
    minecraft/MojangVersionFormat.cpp
    minecraft/MojangVersionFormat.h
    minecraft/Rule.cpp
    minecraft/Rule.h
    minecraft/OneSixVersionFormat.cpp
    minecraft/OneSixVersionFormat.h
    minecraft/ParseUtils.cpp
    minecraft/ParseUtils.h
    minecraft/ProfileUtils.cpp
    minecraft/ProfileUtils.h
    minecraft/Library.cpp
    minecraft/Library.h
    minecraft/MojangDownloadInfo.h
    minecraft/VanillaInstanceCreationTask.cpp
    minecraft/VanillaInstanceCreationTask.h
    minecraft/VersionFile.cpp
    minecraft/VersionFile.h
    minecraft/VersionFilterData.h
    minecraft/VersionFilterData.cpp
    minecraft/World.h
    minecraft/World.cpp
    minecraft/WorldList.h
    minecraft/WorldList.cpp

    minecraft/mod/MetadataHandler.h
    minecraft/mod/Mod.h
    minecraft/mod/Mod.cpp
    minecraft/mod/ModDetails.h
    minecraft/mod/ModFolderModel.h
    minecraft/mod/ModFolderModel.cpp
    minecraft/mod/Resource.h
    minecraft/mod/Resource.cpp
    minecraft/mod/ResourceFolderModel.h
    minecraft/mod/ResourceFolderModel.cpp
    minecraft/mod/DataPack.h
    minecraft/mod/DataPack.cpp
    minecraft/mod/ResourcePack.h
    minecraft/mod/ResourcePack.cpp
    minecraft/mod/ResourcePackFolderModel.h
    minecraft/mod/ResourcePackFolderModel.cpp
    minecraft/mod/TexturePack.h
    minecraft/mod/TexturePack.cpp
    minecraft/mod/ShaderPack.h
    minecraft/mod/ShaderPack.cpp
    minecraft/mod/WorldSave.h
    minecraft/mod/WorldSave.cpp
    minecraft/mod/TexturePackFolderModel.h
    minecraft/mod/TexturePackFolderModel.cpp
    minecraft/mod/ShaderPackFolderModel.h
    minecraft/mod/tasks/ResourceFolderLoadTask.h
    minecraft/mod/tasks/ResourceFolderLoadTask.cpp
    minecraft/mod/tasks/LocalModParseTask.h
    minecraft/mod/tasks/LocalModParseTask.cpp
    minecraft/mod/tasks/LocalResourceUpdateTask.h
    minecraft/mod/tasks/LocalResourceUpdateTask.cpp
    minecraft/mod/tasks/LocalDataPackParseTask.h
    minecraft/mod/tasks/LocalDataPackParseTask.cpp
    minecraft/mod/tasks/LocalResourcePackParseTask.h
    minecraft/mod/tasks/LocalResourcePackParseTask.cpp
    minecraft/mod/tasks/LocalTexturePackParseTask.h
    minecraft/mod/tasks/LocalTexturePackParseTask.cpp
    minecraft/mod/tasks/LocalShaderPackParseTask.h
    minecraft/mod/tasks/LocalShaderPackParseTask.cpp
    minecraft/mod/tasks/LocalWorldSaveParseTask.h
    minecraft/mod/tasks/LocalWorldSaveParseTask.cpp
    minecraft/mod/tasks/LocalResourceParse.h
    minecraft/mod/tasks/LocalResourceParse.cpp
    minecraft/mod/tasks/GetModDependenciesTask.h
    minecraft/mod/tasks/GetModDependenciesTask.cpp

    # Assets
    minecraft/AssetsUtils.h
    minecraft/AssetsUtils.cpp

    # Minecraft skins
    minecraft/skins/CapeChange.cpp
    minecraft/skins/CapeChange.h
    minecraft/skins/SkinUpload.cpp
    minecraft/skins/SkinUpload.h
    minecraft/skins/SkinDelete.cpp
    minecraft/skins/SkinDelete.h
    minecraft/skins/SkinModel.cpp
    minecraft/skins/SkinModel.h
    minecraft/skins/SkinList.cpp
    minecraft/skins/SkinList.h

    minecraft/Agent.h)

# the screenshots feature
set(SCREENSHOTS_SOURCES
    screenshots/Screenshot.h
    screenshots/ImgurUpload.h
    screenshots/ImgurUpload.cpp
    screenshots/ImgurAlbumCreation.h
    screenshots/ImgurAlbumCreation.cpp
)

set(TASKS_SOURCES
    # Tasks
    tasks/Task.h
    tasks/Task.cpp
    tasks/ConcurrentTask.h
    tasks/ConcurrentTask.cpp
    tasks/SequentialTask.h
    tasks/SequentialTask.cpp
    tasks/MultipleOptionsTask.h
    tasks/MultipleOptionsTask.cpp
)

set(SETTINGS_SOURCES
    # Settings
    settings/INIFile.cpp
    settings/INIFile.h
    settings/INISettingsObject.cpp
    settings/INISettingsObject.h
    settings/OverrideSetting.cpp
    settings/OverrideSetting.h
    settings/PassthroughSetting.cpp
    settings/PassthroughSetting.h
    settings/Setting.cpp
    settings/Setting.h
    settings/SettingsObject.cpp
    settings/SettingsObject.h
)

set(JAVA_SOURCES
    java/JavaChecker.h
    java/JavaChecker.cpp
    java/JavaInstall.h
    java/JavaInstall.cpp
    java/JavaInstallList.h
    java/JavaInstallList.cpp
    java/JavaUtils.h
    java/JavaUtils.cpp
    java/JavaVersion.h
    java/JavaVersion.cpp

    java/JavaMetadata.h
    java/JavaMetadata.cpp
    java/download/ArchiveDownloadTask.cpp
    java/download/ArchiveDownloadTask.h
    java/download/ManifestDownloadTask.cpp
    java/download/ManifestDownloadTask.h
    java/download/SymlinkTask.cpp
    java/download/SymlinkTask.h

    ui/java/InstallJavaDialog.h
    ui/java/InstallJavaDialog.cpp
    ui/java/VersionList.h
    ui/java/VersionList.cpp
)

set(TRANSLATIONS_SOURCES
    translations/TranslationsModel.h
    translations/TranslationsModel.cpp
    translations/POTranslator.h
    translations/POTranslator.cpp
)

set(TOOLS_SOURCES
    # Tools
    tools/BaseExternalTool.cpp
    tools/BaseExternalTool.h
    tools/BaseProfiler.cpp
    tools/BaseProfiler.h
    tools/JProfiler.cpp
    tools/JProfiler.h
    tools/JVisualVM.cpp
    tools/JVisualVM.h
    tools/MCEditTool.cpp
    tools/MCEditTool.h
    tools/GenericProfiler.cpp
    tools/GenericProfiler.h
)

set(META_SOURCES
    # Metadata sources
    meta/JsonFormat.cpp
    meta/JsonFormat.h
    meta/BaseEntity.cpp
    meta/BaseEntity.h
    meta/VersionList.cpp
    meta/VersionList.h
    meta/Version.cpp
    meta/Version.h
    meta/Index.cpp
    meta/Index.h
)

set(API_SOURCES
    modplatform/ModIndex.h
    modplatform/ModIndex.cpp

    modplatform/ResourceAPI.h

    modplatform/EnsureMetadataTask.h
    modplatform/EnsureMetadataTask.cpp

    modplatform/CheckUpdateTask.h

    modplatform/flame/FlameAPI.h
    modplatform/flame/FlameAPI.cpp
    modplatform/modrinth/ModrinthAPI.h
    modplatform/modrinth/ModrinthAPI.cpp
    modplatform/helpers/NetworkResourceAPI.h
    modplatform/helpers/NetworkResourceAPI.cpp
    modplatform/helpers/HashUtils.h
    modplatform/helpers/HashUtils.cpp
    modplatform/helpers/OverrideUtils.h
    modplatform/helpers/OverrideUtils.cpp

    modplatform/helpers/ExportToModList.h
    modplatform/helpers/ExportToModList.cpp
)

set(FTB_SOURCES
    modplatform/legacy_ftb/PackFetchTask.h
    modplatform/legacy_ftb/PackFetchTask.cpp
    modplatform/legacy_ftb/PackInstallTask.h
    modplatform/legacy_ftb/PackInstallTask.cpp
    modplatform/legacy_ftb/PrivatePackManager.h
    modplatform/legacy_ftb/PrivatePackManager.cpp

    modplatform/legacy_ftb/PackHelpers.h

    modplatform/import_ftb/PackInstallTask.h
    modplatform/import_ftb/PackInstallTask.cpp
    modplatform/import_ftb/PackHelpers.h
    modplatform/import_ftb/PackHelpers.cpp
)

set(FLAME_SOURCES
    # Flame
    modplatform/flame/FlamePackIndex.cpp
    modplatform/flame/FlamePackIndex.h
    modplatform/flame/FlameModIndex.cpp
    modplatform/flame/FlameModIndex.h
    modplatform/flame/PackManifest.h
    modplatform/flame/PackManifest.cpp
    modplatform/flame/FileResolvingTask.h
    modplatform/flame/FileResolvingTask.cpp
    modplatform/flame/FlameCheckUpdate.cpp
    modplatform/flame/FlameCheckUpdate.h
    modplatform/flame/FlameInstanceCreationTask.h
    modplatform/flame/FlameInstanceCreationTask.cpp
    modplatform/flame/FlamePackExportTask.h
    modplatform/flame/FlamePackExportTask.cpp
)

set(MODRINTH_SOURCES
    modplatform/modrinth/ModrinthPackIndex.cpp
    modplatform/modrinth/ModrinthPackIndex.h
    modplatform/modrinth/ModrinthPackManifest.cpp
    modplatform/modrinth/ModrinthPackManifest.h
    modplatform/modrinth/ModrinthCheckUpdate.cpp
    modplatform/modrinth/ModrinthCheckUpdate.h
    modplatform/modrinth/ModrinthInstanceCreationTask.cpp
    modplatform/modrinth/ModrinthInstanceCreationTask.h
    modplatform/modrinth/ModrinthPackExportTask.cpp
    modplatform/modrinth/ModrinthPackExportTask.h
)

set(PACKWIZ_SOURCES
    modplatform/packwiz/Packwiz.h
    modplatform/packwiz/Packwiz.cpp
)


set(TECHNIC_SOURCES
    modplatform/technic/SingleZipPackInstallTask.h
    modplatform/technic/SingleZipPackInstallTask.cpp
    modplatform/technic/SolderPackInstallTask.h
    modplatform/technic/SolderPackInstallTask.cpp
    modplatform/technic/SolderPackManifest.h
    modplatform/technic/SolderPackManifest.cpp
    modplatform/technic/TechnicPackProcessor.h
    modplatform/technic/TechnicPackProcessor.cpp
)

set(ATLAUNCHER_SOURCES
    modplatform/atlauncher/ATLPackIndex.cpp
    modplatform/atlauncher/ATLPackIndex.h
    modplatform/atlauncher/ATLPackInstallTask.cpp
    modplatform/atlauncher/ATLPackInstallTask.h
    modplatform/atlauncher/ATLPackManifest.cpp
    modplatform/atlauncher/ATLPackManifest.h
    modplatform/atlauncher/ATLShareCode.cpp
    modplatform/atlauncher/ATLShareCode.h
)

set(LINKEXE_SOURCES
    WindowsConsole.cpp
    WindowsConsole.h

    filelink/FileLink.h
    filelink/FileLink.cpp
    FileSystem.h
    FileSystem.cpp
    Exception.h
    StringUtils.h
    StringUtils.cpp
    DesktopServices.h
    DesktopServices.cpp
)

set(PRISMUPDATER_SOURCES
    updater/prismupdater/PrismUpdater.h
    updater/prismupdater/PrismUpdater.cpp
    updater/prismupdater/UpdaterDialogs.h
    updater/prismupdater/UpdaterDialogs.cpp
    updater/prismupdater/GitHubRelease.h
    updater/prismupdater/GitHubRelease.cpp

    Json.h
    Json.cpp
    FileSystem.h
    FileSystem.cpp
    StringUtils.h
    StringUtils.cpp
    DesktopServices.h
    DesktopServices.cpp
    Version.h
    Version.cpp
    Markdown.h
    Markdown.cpp

    # Zip
    MMCZip.h
    MMCZip.cpp

    # Time
    MMCTime.h
    MMCTime.cpp

    net/ByteArraySink.h
    net/ChecksumValidator.h
    net/Download.cpp
    net/Download.h
    net/FileSink.cpp
    net/FileSink.h
    net/HttpMetaCache.cpp
    net/HttpMetaCache.h
    net/Logging.h
    net/Logging.cpp
    net/NetRequest.cpp
    net/NetRequest.h
    net/NetJob.cpp
    net/NetJob.h
    net/NetUtils.h
    net/Sink.h
    net/Validator.h
    net/HeaderProxy.h
    net/RawHeaderProxy.h

    ui/dialogs/ProgressDialog.cpp
    ui/dialogs/ProgressDialog.h
    ui/widgets/SubTaskProgressBar.h
    ui/widgets/SubTaskProgressBar.cpp

)

######## Logging categories ########

ecm_qt_declare_logging_category(CORE_SOURCES
    HEADER Logging.h
    IDENTIFIER authCredentials
    CATEGORY_NAME "launcher.auth.credentials"
    DEFAULT_SEVERITY Warning
    DESCRIPTION "Secrets and credentials for debugging purposes"
    EXPORT "${Launcher_Name}"
)

ecm_qt_export_logging_category(
    IDENTIFIER instanceProfileC
    CATEGORY_NAME "launcher.instance.profile"
    DEFAULT_SEVERITY Debug
    DESCRIPTION "Profile actions"
    EXPORT "${Launcher_Name}"
)

ecm_qt_export_logging_category(
    IDENTIFIER instanceProfileResolveC
    CATEGORY_NAME "launcher.instance.profile.resolve"
    DEFAULT_SEVERITY Debug
    DESCRIPTION "Profile component resolusion actions"
    EXPORT "${Launcher_Name}"
)

ecm_qt_export_logging_category(
    IDENTIFIER taskLogC
    CATEGORY_NAME "launcher.task"
    DEFAULT_SEVERITY Debug
    DESCRIPTION "Task actions"
    EXPORT "${Launcher_Name}"
)

ecm_qt_export_logging_category(
    IDENTIFIER taskNetLogC
    CATEGORY_NAME "launcher.task.net"
    DEFAULT_SEVERITY Debug
    DESCRIPTION "Task network action"
    EXPORT "${Launcher_Name}"
)

ecm_qt_export_logging_category(
    IDENTIFIER taskDownloadLogC
    CATEGORY_NAME "launcher.task.net.download"
    DEFAULT_SEVERITY Debug
    DESCRIPTION "Task network download actions"
    EXPORT "${Launcher_Name}"
)
ecm_qt_export_logging_category(
    IDENTIFIER taskUploadLogC
    CATEGORY_NAME "launcher.task.net.upload"
    DEFAULT_SEVERITY Debug
    DESCRIPTION "Task network upload actions"
    EXPORT "${Launcher_Name}"
)

ecm_qt_export_logging_category(
    IDENTIFIER taskMetaCacheLogC
    CATEGORY_NAME "launcher.task.net.metacache"
    DEFAULT_SEVERITY Debug
    DESCRIPTION "task network meta-cache actions"
    EXPORT "${Launcher_Name}"
)

ecm_qt_export_logging_category(
    IDENTIFIER taskHttpMetaCacheLogC
    CATEGORY_NAME "launcher.task.net.metacache.http"
    DEFAULT_SEVERITY Debug
    DESCRIPTION "task network http meta-cache actions"
    EXPORT "${Launcher_Name}"
)



if(KDE_INSTALL_LOGGINGCATEGORIESDIR)  # only install if there is a standard path for this
    ecm_qt_install_logging_categories(
        EXPORT "${Launcher_Name}"
        DESTINATION "${KDE_INSTALL_LOGGINGCATEGORIESDIR}"
    )
endif()

################################ COMPILE ################################

set(LOGIC_SOURCES
    ${CORE_SOURCES}
    ${PATHMATCHER_SOURCES}
    ${NET_SOURCES}
    ${LAUNCH_SOURCES}
    ${UPDATE_SOURCES}
    ${NEWS_SOURCES}
    ${MINECRAFT_SOURCES}
    ${SCREENSHOTS_SOURCES}
    ${TASKS_SOURCES}
    ${SETTINGS_SOURCES}
    ${JAVA_SOURCES}
    ${TRANSLATIONS_SOURCES}
    ${TOOLS_SOURCES}
    ${META_SOURCES}
    ${ICONS_SOURCES}
    ${API_SOURCES}
    ${FTB_SOURCES}
    ${FLAME_SOURCES}
    ${MODRINTH_SOURCES}
    ${PACKWIZ_SOURCES}
    ${TECHNIC_SOURCES}
    ${ATLAUNCHER_SOURCES}
)

if(APPLE AND Launcher_ENABLE_UPDATER)
    set (LOGIC_SOURCES ${LOGIC_SOURCES} ${MAC_UPDATE_SOURCES})
else()
    set (LOGIC_SOURCES ${LOGIC_SOURCES} ${PRISM_UPDATE_SOURCES})
endif()

SET(LAUNCHER_SOURCES
    # Application base
    Application.h
    Application.cpp
    DataMigrationTask.h
    DataMigrationTask.cpp
    ApplicationMessage.h
    ApplicationMessage.cpp
    SysInfo.h
    SysInfo.cpp

    # GUI - general utilities
    DesktopServices.h
    DesktopServices.cpp
    VersionProxyModel.h
    VersionProxyModel.cpp
    Markdown.h
    Markdown.cpp

    # Super secret!
    KonamiCode.h
    KonamiCode.cpp

    # Bundled resources
    resources/backgrounds/backgrounds.qrc
    resources/multimc/multimc.qrc
    resources/pe_dark/pe_dark.qrc
    resources/pe_light/pe_light.qrc
    resources/pe_colored/pe_colored.qrc
    resources/pe_blue/pe_blue.qrc
    resources/breeze_dark/breeze_dark.qrc
    resources/breeze_light/breeze_light.qrc
    resources/OSX/OSX.qrc
    resources/iOS/iOS.qrc
    resources/flat/flat.qrc
    resources/flat_white/flat_white.qrc
    resources/documents/documents.qrc
    ../${Launcher_Branding_LogoQRC}

    # Icons
    icons/MMCIcon.h
    icons/MMCIcon.cpp
    icons/IconList.h
    icons/IconList.cpp

    # GUI - windows
    ui/GuiUtil.h
    ui/GuiUtil.cpp
    ui/MainWindow.h
    ui/MainWindow.cpp
    ui/InstanceWindow.h
    ui/InstanceWindow.cpp

    # FIXME: maybe find a better home for this.
    FileIgnoreProxy.cpp
    FileIgnoreProxy.h
    FastFileIconProvider.cpp
    FastFileIconProvider.h

    # GUI - setup wizard
    ui/setupwizard/SetupWizard.h
    ui/setupwizard/SetupWizard.cpp
    ui/setupwizard/BaseWizardPage.h
    ui/setupwizard/JavaWizardPage.cpp
    ui/setupwizard/JavaWizardPage.h
    ui/setupwizard/LanguageWizardPage.cpp
    ui/setupwizard/LanguageWizardPage.h
    ui/setupwizard/PasteWizardPage.cpp
    ui/setupwizard/PasteWizardPage.h
    ui/setupwizard/ThemeWizardPage.cpp
    ui/setupwizard/ThemeWizardPage.h
    ui/setupwizard/AutoJavaWizardPage.cpp
    ui/setupwizard/AutoJavaWizardPage.h
    ui/setupwizard/LoginWizardPage.cpp
    ui/setupwizard/LoginWizardPage.h

    # GUI - themes
    ui/themes/FusionTheme.cpp
    ui/themes/FusionTheme.h
    ui/themes/BrightTheme.cpp
    ui/themes/BrightTheme.h
    ui/themes/CustomTheme.cpp
    ui/themes/CustomTheme.h
    ui/themes/DarkTheme.cpp
    ui/themes/DarkTheme.h
    ui/themes/ITheme.cpp
    ui/themes/ITheme.h
    ui/themes/HintOverrideProxyStyle.cpp
    ui/themes/HintOverrideProxyStyle.h
    ui/themes/SystemTheme.cpp
    ui/themes/SystemTheme.h
    ui/themes/IconTheme.cpp
    ui/themes/IconTheme.h
    ui/themes/ThemeManager.cpp
    ui/themes/ThemeManager.h
    ui/themes/CatPack.cpp
    ui/themes/CatPack.h
    ui/themes/CatPainter.cpp
    ui/themes/CatPainter.h

    # Processes
    LaunchController.h
    LaunchController.cpp

    # page provider for instances
    InstancePageProvider.h

    # Common java checking UI
    JavaCommon.h
    JavaCommon.cpp

    # GUI - paged dialog base
    ui/pages/BasePage.h
    ui/pages/BasePageContainer.h
    ui/pages/BasePageProvider.h

    # GUI - instance pages
    ui/pages/instance/ExternalResourcesPage.cpp
    ui/pages/instance/ExternalResourcesPage.h
    ui/pages/instance/GameOptionsPage.cpp
    ui/pages/instance/GameOptionsPage.h
    ui/pages/instance/VersionPage.cpp
    ui/pages/instance/VersionPage.h
    ui/pages/instance/ManagedPackPage.cpp
    ui/pages/instance/ManagedPackPage.h
    ui/pages/instance/TexturePackPage.h
    ui/pages/instance/TexturePackPage.cpp
    ui/pages/instance/ResourcePackPage.h
    ui/pages/instance/ResourcePackPage.cpp
    ui/pages/instance/ShaderPackPage.h
    ui/pages/instance/ShaderPackPage.cpp
    ui/pages/instance/ModFolderPage.cpp
    ui/pages/instance/ModFolderPage.h
    ui/pages/instance/NotesPage.cpp
    ui/pages/instance/NotesPage.h
    ui/pages/instance/LogPage.cpp
    ui/pages/instance/LogPage.h
    ui/pages/instance/InstanceSettingsPage.cpp
    ui/pages/instance/InstanceSettingsPage.h
    ui/pages/instance/ScreenshotsPage.cpp
    ui/pages/instance/ScreenshotsPage.h
    ui/pages/instance/OtherLogsPage.cpp
    ui/pages/instance/OtherLogsPage.h
    ui/pages/instance/ServersPage.cpp
    ui/pages/instance/ServersPage.h
    ui/pages/instance/WorldListPage.cpp
    ui/pages/instance/WorldListPage.h

    # GUI - global settings pages
    ui/pages/global/AccountListPage.cpp
    ui/pages/global/AccountListPage.h
    ui/pages/global/CustomCommandsPage.cpp
    ui/pages/global/CustomCommandsPage.h
    ui/pages/global/EnvironmentVariablesPage.cpp
    ui/pages/global/EnvironmentVariablesPage.h
    ui/pages/global/ExternalToolsPage.cpp
    ui/pages/global/ExternalToolsPage.h
    ui/pages/global/JavaPage.cpp
    ui/pages/global/JavaPage.h
    ui/pages/global/LanguagePage.cpp
    ui/pages/global/LanguagePage.h
    ui/pages/global/MinecraftPage.cpp
    ui/pages/global/MinecraftPage.h
    ui/pages/global/LauncherPage.cpp
    ui/pages/global/LauncherPage.h
    ui/pages/global/ProxyPage.cpp
    ui/pages/global/ProxyPage.h
    ui/pages/global/APIPage.cpp
    ui/pages/global/APIPage.h

    # GUI - platform pages
    ui/pages/modplatform/CustomPage.cpp
    ui/pages/modplatform/CustomPage.h

    ui/pages/modplatform/ResourcePage.cpp
    ui/pages/modplatform/ResourcePage.h
    ui/pages/modplatform/ResourceModel.cpp
    ui/pages/modplatform/ResourceModel.h

    ui/pages/modplatform/ModPage.cpp
    ui/pages/modplatform/ModPage.h
    ui/pages/modplatform/ModModel.cpp
    ui/pages/modplatform/ModModel.h

    ui/pages/modplatform/ResourcePackPage.cpp
    ui/pages/modplatform/ResourcePackModel.cpp

    # Needed for MOC to find them without a corresponding .cpp
    ui/pages/modplatform/TexturePackPage.h
    ui/pages/modplatform/TexturePackModel.cpp

    ui/pages/modplatform/ShaderPackPage.cpp
    ui/pages/modplatform/ShaderPackModel.cpp


    ui/pages/modplatform/ModpackProviderBasePage.h

    ui/pages/modplatform/atlauncher/AtlFilterModel.cpp
    ui/pages/modplatform/atlauncher/AtlFilterModel.h
    ui/pages/modplatform/atlauncher/AtlListModel.cpp
    ui/pages/modplatform/atlauncher/AtlListModel.h
    ui/pages/modplatform/atlauncher/AtlOptionalModDialog.cpp
    ui/pages/modplatform/atlauncher/AtlOptionalModDialog.h
    ui/pages/modplatform/atlauncher/AtlPage.cpp
    ui/pages/modplatform/atlauncher/AtlPage.h
    ui/pages/modplatform/atlauncher/AtlUserInteractionSupportImpl.cpp
    ui/pages/modplatform/atlauncher/AtlUserInteractionSupportImpl.h

    ui/pages/modplatform/legacy_ftb/Page.cpp
    ui/pages/modplatform/legacy_ftb/Page.h
    ui/pages/modplatform/legacy_ftb/ListModel.h
    ui/pages/modplatform/legacy_ftb/ListModel.cpp

    ui/pages/modplatform/import_ftb/ImportFTBPage.cpp
    ui/pages/modplatform/import_ftb/ImportFTBPage.h
    ui/pages/modplatform/import_ftb/ListModel.h
    ui/pages/modplatform/import_ftb/ListModel.cpp

    ui/pages/modplatform/flame/FlameModel.cpp
    ui/pages/modplatform/flame/FlameModel.h
    ui/pages/modplatform/flame/FlamePage.cpp
    ui/pages/modplatform/flame/FlamePage.h
    ui/pages/modplatform/flame/FlameResourceModels.cpp
    ui/pages/modplatform/flame/FlameResourceModels.h
    ui/pages/modplatform/flame/FlameResourcePages.cpp
    ui/pages/modplatform/flame/FlameResourcePages.h

    ui/pages/modplatform/modrinth/ModrinthPage.cpp
    ui/pages/modplatform/modrinth/ModrinthPage.h
    ui/pages/modplatform/modrinth/ModrinthModel.cpp
    ui/pages/modplatform/modrinth/ModrinthModel.h

    ui/pages/modplatform/technic/TechnicModel.cpp
    ui/pages/modplatform/technic/TechnicModel.h
    ui/pages/modplatform/technic/TechnicPage.cpp
    ui/pages/modplatform/technic/TechnicPage.h

    ui/pages/modplatform/ImportPage.cpp
    ui/pages/modplatform/ImportPage.h

    ui/pages/modplatform/OptionalModDialog.cpp
    ui/pages/modplatform/OptionalModDialog.h

    ui/pages/modplatform/modrinth/ModrinthResourceModels.cpp
    ui/pages/modplatform/modrinth/ModrinthResourceModels.h
    ui/pages/modplatform/modrinth/ModrinthResourcePages.cpp
    ui/pages/modplatform/modrinth/ModrinthResourcePages.h

    # GUI - dialogs
    ui/dialogs/AboutDialog.cpp
    ui/dialogs/AboutDialog.h
    ui/dialogs/ProfileSelectDialog.cpp
    ui/dialogs/ProfileSelectDialog.h
    ui/dialogs/ProfileSetupDialog.cpp
    ui/dialogs/ProfileSetupDialog.h
    ui/dialogs/CopyInstanceDialog.cpp
    ui/dialogs/CopyInstanceDialog.h
    ui/dialogs/CustomMessageBox.cpp
    ui/dialogs/CustomMessageBox.h
    ui/dialogs/ExportInstanceDialog.cpp
    ui/dialogs/ExportInstanceDialog.h
    ui/dialogs/ExportPackDialog.cpp
    ui/dialogs/ExportPackDialog.h
    ui/dialogs/ExportToModListDialog.cpp
    ui/dialogs/ExportToModListDialog.h
    ui/dialogs/IconPickerDialog.cpp
    ui/dialogs/IconPickerDialog.h
    ui/dialogs/ImportResourceDialog.cpp
    ui/dialogs/ImportResourceDialog.h
    ui/dialogs/MSALoginDialog.cpp
    ui/dialogs/MSALoginDialog.h
    ui/dialogs/OfflineLoginDialog.cpp
    ui/dialogs/OfflineLoginDialog.h
    ui/dialogs/NewComponentDialog.cpp
    ui/dialogs/NewComponentDialog.h
    ui/dialogs/NewInstanceDialog.cpp
    ui/dialogs/NewInstanceDialog.h
    ui/dialogs/NewsDialog.cpp
    ui/dialogs/NewsDialog.h
    ui/pagedialog/PageDialog.cpp
    ui/pagedialog/PageDialog.h
    ui/dialogs/ProgressDialog.cpp
    ui/dialogs/ProgressDialog.h
    ui/dialogs/ReviewMessageBox.cpp
    ui/dialogs/ReviewMessageBox.h
    ui/dialogs/VersionSelectDialog.cpp
    ui/dialogs/VersionSelectDialog.h
    ui/dialogs/ResourceDownloadDialog.cpp
    ui/dialogs/ResourceDownloadDialog.h
    ui/dialogs/ScrollMessageBox.cpp
    ui/dialogs/ScrollMessageBox.h
    ui/dialogs/BlockedModsDialog.cpp
    ui/dialogs/BlockedModsDialog.h
    ui/dialogs/ChooseProviderDialog.h
    ui/dialogs/ChooseProviderDialog.cpp
    ui/dialogs/ResourceUpdateDialog.cpp
    ui/dialogs/ResourceUpdateDialog.h
    ui/dialogs/InstallLoaderDialog.cpp
    ui/dialogs/InstallLoaderDialog.h

    ui/dialogs/skins/SkinManageDialog.cpp
    ui/dialogs/skins/SkinManageDialog.h

    # GUI - widgets
    ui/widgets/CheckComboBox.cpp
    ui/widgets/CheckComboBox.h
    ui/widgets/Common.cpp
    ui/widgets/Common.h
    ui/widgets/CustomCommands.cpp
    ui/widgets/CustomCommands.h
    ui/widgets/EnvironmentVariables.cpp
    ui/widgets/EnvironmentVariables.h
    ui/widgets/FocusLineEdit.cpp
    ui/widgets/FocusLineEdit.h
    ui/widgets/IconLabel.cpp
    ui/widgets/IconLabel.h
    ui/widgets/JavaSettingsWidget.cpp
    ui/widgets/JavaSettingsWidget.h
    ui/widgets/LabeledToolButton.cpp
    ui/widgets/LabeledToolButton.h
    ui/widgets/LanguageSelectionWidget.cpp
    ui/widgets/LanguageSelectionWidget.h
    ui/widgets/LineSeparator.cpp
    ui/widgets/LineSeparator.h
    ui/widgets/LogView.cpp
    ui/widgets/LogView.h
    ui/widgets/InfoFrame.cpp
    ui/widgets/InfoFrame.h
    ui/widgets/ModFilterWidget.cpp
    ui/widgets/ModFilterWidget.h
    ui/widgets/ModListView.cpp
    ui/widgets/ModListView.h
    ui/widgets/PageContainer.cpp
    ui/widgets/PageContainer.h
    ui/widgets/PageContainer_p.h
    ui/widgets/ProjectDescriptionPage.h
    ui/widgets/ProjectDescriptionPage.cpp
    ui/widgets/VariableSizedImageObject.h
    ui/widgets/VariableSizedImageObject.cpp
    ui/widgets/ProjectItem.h
    ui/widgets/ProjectItem.cpp
    ui/widgets/SubTaskProgressBar.h
    ui/widgets/SubTaskProgressBar.cpp
    ui/widgets/VersionListView.cpp
    ui/widgets/VersionListView.h
    ui/widgets/VersionSelectWidget.cpp
    ui/widgets/VersionSelectWidget.h
    ui/widgets/ProgressWidget.h
    ui/widgets/ProgressWidget.cpp
    ui/widgets/WideBar.h
    ui/widgets/WideBar.cpp
    ui/widgets/ThemeCustomizationWidget.h
    ui/widgets/ThemeCustomizationWidget.cpp

    # GUI - instance group view
    ui/instanceview/InstanceProxyModel.cpp
    ui/instanceview/InstanceProxyModel.h
    ui/instanceview/AccessibleInstanceView.cpp
    ui/instanceview/AccessibleInstanceView.h
    ui/instanceview/AccessibleInstanceView_p.h
    ui/instanceview/InstanceView.cpp
    ui/instanceview/InstanceView.h
    ui/instanceview/InstanceDelegate.cpp
    ui/instanceview/InstanceDelegate.h
    ui/instanceview/VisualGroup.cpp
    ui/instanceview/VisualGroup.h
)

if (NOT Apple)
set(LAUNCHER_SOURCES
    ${LAUNCHER_SOURCES}

    ui/dialogs/UpdateAvailableDialog.h
    ui/dialogs/UpdateAvailableDialog.cpp
)
endif()

if(WIN32)
    set(LAUNCHER_SOURCES
        WindowsConsole.cpp
        WindowsConsole.h
        ${LAUNCHER_SOURCES}
    )
endif()

qt_wrap_ui(LAUNCHER_UI
    ui/MainWindow.ui
    ui/setupwizard/PasteWizardPage.ui
    ui/setupwizard/AutoJavaWizardPage.ui
    ui/setupwizard/LoginWizardPage.ui
    ui/setupwizard/ThemeWizardPage.ui
    ui/pages/global/AccountListPage.ui
    ui/pages/global/JavaPage.ui
    ui/pages/global/LauncherPage.ui
    ui/pages/global/APIPage.ui
    ui/pages/global/ProxyPage.ui
    ui/pages/global/MinecraftPage.ui
    ui/pages/global/ExternalToolsPage.ui
    ui/pages/instance/ExternalResourcesPage.ui
    ui/pages/instance/NotesPage.ui
    ui/pages/instance/LogPage.ui
    ui/pages/instance/ServersPage.ui
    ui/pages/instance/GameOptionsPage.ui
    ui/pages/instance/OtherLogsPage.ui
    ui/pages/instance/InstanceSettingsPage.ui
    ui/pages/instance/VersionPage.ui
    ui/pages/instance/ManagedPackPage.ui
    ui/pages/instance/WorldListPage.ui
    ui/pages/instance/ScreenshotsPage.ui
    ui/pages/modplatform/atlauncher/AtlOptionalModDialog.ui
    ui/pages/modplatform/atlauncher/AtlPage.ui
    ui/pages/modplatform/CustomPage.ui
    ui/pages/modplatform/ResourcePage.ui
    ui/pages/modplatform/flame/FlamePage.ui
    ui/pages/modplatform/legacy_ftb/Page.ui
    ui/pages/modplatform/import_ftb/ImportFTBPage.ui
    ui/pages/modplatform/ImportPage.ui
    ui/pages/modplatform/OptionalModDialog.ui
    ui/pages/modplatform/modrinth/ModrinthPage.ui
    ui/pages/modplatform/technic/TechnicPage.ui
    ui/widgets/InstanceCardWidget.ui
    ui/widgets/CustomCommands.ui
    ui/widgets/EnvironmentVariables.ui
    ui/widgets/InfoFrame.ui
    ui/widgets/ModFilterWidget.ui
    ui/widgets/SubTaskProgressBar.ui
    ui/widgets/ThemeCustomizationWidget.ui
    ui/dialogs/CopyInstanceDialog.ui
    ui/dialogs/ProfileSetupDialog.ui
    ui/dialogs/ProgressDialog.ui
    ui/dialogs/NewInstanceDialog.ui
    ui/dialogs/NewComponentDialog.ui
    ui/dialogs/NewsDialog.ui
    ui/dialogs/ProfileSelectDialog.ui
    ui/dialogs/ExportInstanceDialog.ui
    ui/dialogs/ExportPackDialog.ui
    ui/dialogs/ExportToModListDialog.ui
    ui/dialogs/IconPickerDialog.ui
    ui/dialogs/ImportResourceDialog.ui
    ui/dialogs/MSALoginDialog.ui
    ui/dialogs/OfflineLoginDialog.ui
    ui/dialogs/AboutDialog.ui
    ui/dialogs/ReviewMessageBox.ui
    ui/dialogs/ScrollMessageBox.ui
    ui/dialogs/BlockedModsDialog.ui
    ui/dialogs/ChooseProviderDialog.ui

    ui/dialogs/skins/SkinManageDialog.ui
)

qt_wrap_ui(PRISM_UPDATE_UI
    ui/dialogs/UpdateAvailableDialog.ui
)

if (NOT Apple)
    set (LAUNCHER_UI ${LAUNCHER_UI} ${PRISM_UPDATE_UI})
endif()

qt_add_resources(LAUNCHER_RESOURCES
    resources/backgrounds/backgrounds.qrc
    resources/multimc/multimc.qrc
    resources/pe_dark/pe_dark.qrc
    resources/pe_light/pe_light.qrc
    resources/pe_colored/pe_colored.qrc
    resources/pe_blue/pe_blue.qrc
    resources/breeze_dark/breeze_dark.qrc
    resources/breeze_light/breeze_light.qrc
    resources/OSX/OSX.qrc
    resources/iOS/iOS.qrc
    resources/flat/flat.qrc
    resources/documents/documents.qrc
    ../${Launcher_Branding_LogoQRC}
)

qt_wrap_ui(PRISMUPDATER_UI
    updater/prismupdater/SelectReleaseDialog.ui
    ui/widgets/SubTaskProgressBar.ui
    ui/dialogs/ProgressDialog.ui
)

######## Windows resource files ########
if(WIN32)
    set(LAUNCHER_RCS ${CMAKE_CURRENT_BINARY_DIR}/../${Launcher_Branding_WindowsRC})
endif()

include(CompilerWarnings)

# Add executable
add_library(Launcher_logic STATIC ${LOGIC_SOURCES} ${LAUNCHER_SOURCES} ${LAUNCHER_UI} ${LAUNCHER_RESOURCES})
set_project_warnings(Launcher_logic
    "${Launcher_MSVC_WARNINGS}"
    "${Launcher_CLANG_WARNINGS}"
    "${Launcher_GCC_WARNINGS}")
target_include_directories(Launcher_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(Launcher_logic PUBLIC LAUNCHER_APPLICATION)
target_link_libraries(Launcher_logic
    systeminfo
    Launcher_murmur2
    nbt++
    ${ZLIB_LIBRARIES}
    tomlplusplus::tomlplusplus
    qdcss
    BuildConfig
    Qt${QT_VERSION_MAJOR}::Widgets
    ghcFilesystem::ghc_filesystem
)

if (UNIX AND NOT CYGWIN AND NOT APPLE)
    target_link_libraries(Launcher_logic
        gamemode
    )
endif()

target_link_libraries(Launcher_logic
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Xml
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::NetworkAuth
    ${Launcher_QT_DBUS}
    ${Launcher_QT_LIBS}
)
target_link_libraries(Launcher_logic
    QuaZip::QuaZip
    cmark::cmark
    LocalPeer
    Launcher_rainbow
)
if (TARGET ${Launcher_QT_DBUS})
    add_compile_definitions(WITH_QTDBUS)
endif()

if(APPLE)
    set(CMAKE_MACOSX_RPATH 1)
    set(CMAKE_INSTALL_RPATH "@loader_path/../Frameworks/")

    if(Launcher_ENABLE_UPDATER)
      file(DOWNLOAD ${MACOSX_SPARKLE_DOWNLOAD_URL} ${CMAKE_BINARY_DIR}/Sparkle.tar.xz EXPECTED_HASH SHA256=${MACOSX_SPARKLE_SHA256})
      file(ARCHIVE_EXTRACT INPUT ${CMAKE_BINARY_DIR}/Sparkle.tar.xz DESTINATION ${CMAKE_BINARY_DIR}/frameworks/Sparkle)

      find_library(SPARKLE_FRAMEWORK Sparkle "${CMAKE_BINARY_DIR}/frameworks/Sparkle")
      add_compile_definitions(SPARKLE_ENABLED)
    endif()

    target_link_libraries(Launcher_logic
        "-framework AppKit"
        "-framework Carbon"
        "-framework Foundation"
        "-framework ApplicationServices"
    )
    if(Launcher_ENABLE_UPDATER)
      target_link_libraries(Launcher_logic ${SPARKLE_FRAMEWORK})
    endif()
endif()

target_link_libraries(Launcher_logic)

add_executable(${Launcher_Name} MACOSX_BUNDLE WIN32 main.cpp ${LAUNCHER_RCS})
target_link_libraries(${Launcher_Name} Launcher_logic)

if(DEFINED Launcher_APP_BINARY_NAME)
    set_target_properties(${Launcher_Name} PROPERTIES OUTPUT_NAME "${Launcher_APP_BINARY_NAME}")
endif()
if(DEFINED Launcher_BINARY_RPATH)
    SET_TARGET_PROPERTIES(${Launcher_Name} PROPERTIES INSTALL_RPATH "${Launcher_BINARY_RPATH}")
endif()

if(DEFINED Launcher_APP_BINARY_DEFS)
    target_compile_definitions(${Launcher_Name} PRIVATE ${Launcher_APP_BINARY_DEFS})
    target_compile_definitions(Launcher_logic PRIVATE ${Launcher_APP_BINARY_DEFS})
endif()

install(TARGETS ${Launcher_Name}
    BUNDLE DESTINATION "." COMPONENT Runtime
    LIBRARY DESTINATION ${LIBRARY_DEST_DIR} COMPONENT Runtime
    RUNTIME DESTINATION ${BINARY_DEST_DIR} COMPONENT Runtime
    FRAMEWORK DESTINATION ${FRAMEWORK_DEST_DIR} COMPONENT Runtime
)

if(Launcher_BUILD_UPDATER)
    # Updater
    add_library(prism_updater_logic STATIC ${PRISMUPDATER_SOURCES} ${TASKS_SOURCES} ${PRISMUPDATER_UI})
    target_include_directories(prism_updater_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(prism_updater_logic
        QuaZip::QuaZip
        ${ZLIB_LIBRARIES}
        systeminfo
        BuildConfig
        ghcFilesystem::ghc_filesystem
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
        ${Launcher_QT_LIBS}
        cmark::cmark
    )

    add_executable("${Launcher_Name}_updater" WIN32 updater/prismupdater/updater_main.cpp)
    target_sources("${Launcher_Name}_updater" PRIVATE updater/prismupdater/updater.exe.manifest)
    target_link_libraries("${Launcher_Name}_updater" prism_updater_logic)

    if(DEFINED Launcher_APP_BINARY_NAME)
        set_target_properties("${Launcher_Name}_updater" PROPERTIES OUTPUT_NAME "${Launcher_APP_BINARY_NAME}_updater")
    endif()
    if(DEFINED Launcher_BINARY_RPATH)
        SET_TARGET_PROPERTIES("${Launcher_Name}_updater" PROPERTIES INSTALL_RPATH "${Launcher_BINARY_RPATH}")
    endif()

    install(TARGETS "${Launcher_Name}_updater"
        BUNDLE DESTINATION "." COMPONENT Runtime
        LIBRARY DESTINATION ${LIBRARY_DEST_DIR} COMPONENT Runtime
        RUNTIME DESTINATION ${BINARY_DEST_DIR} COMPONENT Runtime
        FRAMEWORK DESTINATION ${FRAMEWORK_DEST_DIR} COMPONENT Runtime
    )
endif()

if(WIN32 OR (DEFINED Launcher_BUILD_FILELINKER AND Launcher_BUILD_FILELINKER))
    # File link
    add_library(filelink_logic STATIC ${LINKEXE_SOURCES})
    set_project_warnings(filelink_logic
    "${Launcher_MSVC_WARNINGS}"
    "${Launcher_CLANG_WARNINGS}"
    "${Launcher_GCC_WARNINGS}")

    target_include_directories(filelink_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(filelink_logic
        systeminfo
        BuildConfig
        ghcFilesystem::ghc_filesystem
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
        # Qt${QT_VERSION_MAJOR}::Concurrent
        ${Launcher_QT_LIBS}
    )

    add_executable("${Launcher_Name}_filelink" WIN32 filelink/filelink_main.cpp)

    target_sources("${Launcher_Name}_filelink" PRIVATE filelink/filelink.exe.manifest)

    target_link_libraries("${Launcher_Name}_filelink" filelink_logic)

    if(DEFINED Launcher_APP_BINARY_NAME)
        set_target_properties("${Launcher_Name}_filelink" PROPERTIES OUTPUT_NAME "${Launcher_APP_BINARY_NAME}_filelink")
    endif()
    if(DEFINED Launcher_BINARY_RPATH)
        SET_TARGET_PROPERTIES("${Launcher_Name}_filelink" PROPERTIES INSTALL_RPATH "${Launcher_BINARY_RPATH}")
    endif()

    install(TARGETS "${Launcher_Name}_filelink"
        BUNDLE DESTINATION "." COMPONENT Runtime
        LIBRARY DESTINATION ${LIBRARY_DEST_DIR} COMPONENT Runtime
        RUNTIME DESTINATION ${BINARY_DEST_DIR} COMPONENT Runtime
        FRAMEWORK DESTINATION ${FRAMEWORK_DEST_DIR} COMPONENT Runtime
    )
endif()

if (UNIX AND APPLE AND Launcher_ENABLE_UPDATER)
    # Add Sparkle updater
    # It has to be copied here instead of just allowing fixup_bundle to install it, otherwise essential parts of
    # the framework aren't installed
    install(DIRECTORY ${MACOSX_SPARKLE_DIR}/Sparkle.framework DESTINATION ${FRAMEWORK_DEST_DIR} USE_SOURCE_PERMISSIONS)
endif()

#### The bundle mess! ####
# Bundle utilities are used to complete the portable packages - they add all the libraries that would otherwise be missing on the target system.
# NOTE: it seems that this absolutely has to be here, and nowhere else.
if(INSTALL_BUNDLE STREQUAL "full")
    # Add qt.conf - this makes Qt stop looking for things outside the bundle
    install(
        CODE "file(WRITE \"\${CMAKE_INSTALL_PREFIX}/${RESOURCES_DEST_DIR}/qt.conf\" \" \")"
        COMPONENT Runtime
    )
    # add qtlogging.ini as a config file
    install(
        FILES "qtlogging.ini"
        DESTINATION ${CMAKE_INSTALL_PREFIX}/${RESOURCES_DEST_DIR}
        COMPONENT Runtime
    )
    # Bundle plugins
    # Image formats
    install(
        DIRECTORY "${QT_PLUGINS_DIR}/imageformats"
        CONFIGURATIONS Debug RelWithDebInfo ""
        DESTINATION ${PLUGIN_DEST_DIR}
        COMPONENT Runtime
        REGEX "tga|tiff|mng" EXCLUDE
    )
    install(
        DIRECTORY "${QT_PLUGINS_DIR}/imageformats"
        CONFIGURATIONS Release MinSizeRel
        DESTINATION ${PLUGIN_DEST_DIR}
        COMPONENT Runtime
        REGEX "tga|tiff|mng" EXCLUDE
        REGEX "d\\." EXCLUDE
        REGEX "_debug\\." EXCLUDE
        REGEX "\\.dSYM" EXCLUDE
    )
    # Icon engines
    install(
        DIRECTORY "${QT_PLUGINS_DIR}/iconengines"
        CONFIGURATIONS Debug RelWithDebInfo ""
        DESTINATION ${PLUGIN_DEST_DIR}
        COMPONENT Runtime
        REGEX "fontawesome" EXCLUDE
    )
    install(
        DIRECTORY "${QT_PLUGINS_DIR}/iconengines"
        CONFIGURATIONS Release MinSizeRel
        DESTINATION ${PLUGIN_DEST_DIR}
        COMPONENT Runtime
        REGEX "fontawesome" EXCLUDE
        REGEX "d\\." EXCLUDE
        REGEX "_debug\\." EXCLUDE
        REGEX "\\.dSYM" EXCLUDE
    )
    # Platform plugins
    install(
        DIRECTORY "${QT_PLUGINS_DIR}/platforms"
        CONFIGURATIONS Debug RelWithDebInfo ""
        DESTINATION ${PLUGIN_DEST_DIR}
        COMPONENT Runtime
        REGEX "minimal|linuxfb|offscreen" EXCLUDE
    )
    install(
        DIRECTORY "${QT_PLUGINS_DIR}/platforms"
        CONFIGURATIONS Release MinSizeRel
        DESTINATION ${PLUGIN_DEST_DIR}
        COMPONENT Runtime
        REGEX "minimal|linuxfb|offscreen" EXCLUDE
        REGEX "[^2]d\\." EXCLUDE
        REGEX "_debug\\." EXCLUDE
        REGEX "\\.dSYM" EXCLUDE
    )
    # Style plugins
    if(EXISTS "${QT_PLUGINS_DIR}/styles")
        install(
            DIRECTORY "${QT_PLUGINS_DIR}/styles"
            CONFIGURATIONS Debug RelWithDebInfo ""
            DESTINATION ${PLUGIN_DEST_DIR}
            COMPONENT Runtime
        )
        install(
            DIRECTORY "${QT_PLUGINS_DIR}/styles"
            CONFIGURATIONS Release MinSizeRel
            DESTINATION ${PLUGIN_DEST_DIR}
            COMPONENT Runtime
            REGEX "d\\." EXCLUDE
            REGEX "_debug\\." EXCLUDE
            REGEX "\\.dSYM" EXCLUDE
        )
    endif()
    # TLS plugins (Qt 6 only)
    if(EXISTS "${QT_PLUGINS_DIR}/tls")
        install(
            DIRECTORY "${QT_PLUGINS_DIR}/tls"
            CONFIGURATIONS Debug RelWithDebInfo ""
            DESTINATION ${PLUGIN_DEST_DIR}
            COMPONENT Runtime
            PATTERN "*qcertonlybackend*" EXCLUDE
        )
        install(
            DIRECTORY "${QT_PLUGINS_DIR}/tls"
            CONFIGURATIONS Release MinSizeRel
            DESTINATION ${PLUGIN_DEST_DIR}
            COMPONENT Runtime
            REGEX "dd\\." EXCLUDE
            REGEX "_debug\\." EXCLUDE
            REGEX "\\.dSYM" EXCLUDE
            PATTERN "*qcertonlybackend*" EXCLUDE
        )
    endif()
    # Wayland support
    if(EXISTS "${QT_PLUGINS_DIR}/wayland-graphics-integration-client")
        install(
            DIRECTORY "${QT_PLUGINS_DIR}/wayland-graphics-integration-client"
            CONFIGURATIONS Debug RelWithDebInfo ""
            DESTINATION ${PLUGIN_DEST_DIR}
            COMPONENT Runtime
        )
        install(
            DIRECTORY "${QT_PLUGINS_DIR}/wayland-graphics-integration-client"
            CONFIGURATIONS Release MinSizeRel
            DESTINATION ${PLUGIN_DEST_DIR}
            COMPONENT Runtime
            REGEX "dd\\." EXCLUDE
            REGEX "_debug\\." EXCLUDE
            REGEX "\\.dSYM" EXCLUDE
        )
    endif()
    if(EXISTS "${QT_PLUGINS_DIR}/wayland-graphics-integration-server")
        install(
            DIRECTORY "${QT_PLUGINS_DIR}/wayland-graphics-integration-server"
            CONFIGURATIONS Debug RelWithDebInfo ""
            DESTINATION ${PLUGIN_DEST_DIR}
            COMPONENT Runtime
        )
        install(
            DIRECTORY "${QT_PLUGINS_DIR}/wayland-graphics-integration-server"
            CONFIGURATIONS Release MinSizeRel
            DESTINATION ${PLUGIN_DEST_DIR}
            COMPONENT Runtime
            REGEX "dd\\." EXCLUDE
            REGEX "_debug\\." EXCLUDE
            REGEX "\\.dSYM" EXCLUDE
        )
    endif()
    if(EXISTS "${QT_PLUGINS_DIR}/wayland-decoration-client")
        install(
            DIRECTORY "${QT_PLUGINS_DIR}/wayland-decoration-client"
            CONFIGURATIONS Debug RelWithDebInfo ""
            DESTINATION ${PLUGIN_DEST_DIR}
            COMPONENT Runtime
        )
        install(
            DIRECTORY "${QT_PLUGINS_DIR}/wayland-decoration-client"
            CONFIGURATIONS Release MinSizeRel
            DESTINATION ${PLUGIN_DEST_DIR}
            COMPONENT Runtime
            REGEX "dd\\." EXCLUDE
            REGEX "_debug\\." EXCLUDE
            REGEX "\\.dSYM" EXCLUDE
        )
    endif()
    if(EXISTS "${QT_PLUGINS_DIR}/wayland-shell-integration")
        install(
            DIRECTORY "${QT_PLUGINS_DIR}/wayland-shell-integration"
            CONFIGURATIONS Debug RelWithDebInfo ""
            DESTINATION ${PLUGIN_DEST_DIR}
            COMPONENT Runtime
        )
        install(
            DIRECTORY "${QT_PLUGINS_DIR}/wayland-shell-integration"
            CONFIGURATIONS Release MinSizeRel
            DESTINATION ${PLUGIN_DEST_DIR}
            COMPONENT Runtime
            REGEX "dd\\." EXCLUDE
            REGEX "_debug\\." EXCLUDE
            REGEX "\\.dSYM" EXCLUDE
        )
    endif()
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/install_prereqs.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/install_prereqs.cmake"
        @ONLY
    )
    install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/install_prereqs.cmake" COMPONENT Runtime)
endif()
