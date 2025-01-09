function Component() { }

function vcRedistExeName(architecture) {
	switch (systemInfo.currentCpuArchitecture()) {
		case "x86_64":
			return "vc_redist.x64.exe";
		case "arm64":
			return "vc_redist.arm64.exe";
		default:
			throw new Error("You CPU Architecture is not supported! Womp womp");
	}
}

function handleVCRedist() {
	const exeName = vcRedistExeName(architecture);
	const exePath = "@TargetDir@/" + exeName;

	component.addElevatedOperation("Execute", vcpath, "/quiet", "/norestart");
	component.addOperation("Delete", vcpath);
}

Component.prototype.createOperations = function() {
	try {
		// Call default impl first
		component.createOperations();

		handleVCRedist();
	} catch(error) {
		console.log(error);
	}
}
