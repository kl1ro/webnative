import buildAndroidApk from "./android/apk.js";
import buildLinuxAppImage from "./linux/appimage.js";
import buildWindowsZip from "./windows/zip.js";

export const builders = {
  linux: {
    appimage: buildLinuxAppImage,
  },
  windows: {
    zip: buildWindowsZip,
  },
  android: {
    apk: buildAndroidApk,
  },
} as const;

export function getPlatformsDesc() {
  return (
    "Available platforms:\n" +
    Object.entries(builders)
      .map(
        ([platform, targets]) =>
          `  ${platform}, targets: ${Object.keys(targets).join(", ")}`,
      )
      .join("\n")
  );
}

export type SpecificPlatform = keyof typeof builders;
export type Platform = SpecificPlatform | "all";

export type SpecificTarget = {
  [P in SpecificPlatform]: keyof (typeof builders)[P];
}[SpecificPlatform];

export type Target = SpecificTarget | "all";
