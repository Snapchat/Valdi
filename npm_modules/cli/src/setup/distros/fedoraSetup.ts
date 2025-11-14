import { checkCommandExists } from "../../utils/cliUtils";
import { DevSetupHelper } from "../DevSetupHelper";

export async function fedoraSetup(devSetup: DevSetupHelper): Promise<void> {
  await devSetup.runShell('Installing dependencies from dnf', [
    `sudo dnf install zlib-devel git-lfs watchman fontconfig-devel android-tools`,
  ]);

  await devSetup.runShell('Installing ncurses-compat-libs', [
    `sudo dnf install ncurses-compat-libs`,
  ]);

  if (!checkCommandExists('java')) {
    await devSetup.runShell('Installing Java Runtime Environment', ['sudo dnf install default-jre']);
  }
}
