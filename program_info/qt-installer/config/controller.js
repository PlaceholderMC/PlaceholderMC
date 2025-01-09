function Controller() {
	if (installer.isInstaller()) {
		handleTargetDir();
	}
}

function handleTargetDir() {
	const baseTarget = installer.value("TargetDir");
	const os = installer.value("os");

	if (os === "win") {
		installer.setValue("TargetDir", "@HomeDir@/AppData/Local/Programs/" + baseTarget);
		// Default to `C:\Program Files\PrismLauncher` when run as admin
		installer.setValue("AdminTargetDir", "@ApplicationsDir@/" + baseTarget);
		return;
	}

	// TODO: support other operating systems
	installer.setValue("TargetDir", "@ApplicationsDir@/" + baseTarget);
	installer.setValue("AdminTargetDir", "@ApplicationsDir@/" + baseTarget);
}
