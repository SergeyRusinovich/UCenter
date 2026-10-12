#!/bin/sh
# Нотаризация подписанного Developer ID образа (ADR 0019): отправка в Apple, ожидание ответа,
# прикрепление квитанции к образу (staple) и проверка Gatekeeper.
#   tools/notarize.sh build/package/UCenter-1.2.0-Darwin.dmg [профиль, по умолчанию UCenter]
# Профиль с учётными данными создаётся один раз и хранится в связке ключей:
#   xcrun notarytool store-credentials UCenter --apple-id <Apple ID> --team-id D966UR5B6V
# (пароль — пароль приложения с appleid.apple.com; вместо Apple ID можно ключ App Store Connect API)
set -eu
DMG="${1:?usage: tools/notarize.sh <file.dmg> [keychain profile]}"
PROFILE="${2:-UCenter}"
[ -f "$DMG" ] || { echo "No such file: $DMG" >&2; exit 1; }
codesign --verify --strict "$DMG" || { echo "$DMG is not signed: configure with -DUCENTER_CODESIGN_IDENTITY=..." >&2; exit 1; }
xcrun notarytool submit "$DMG" --keychain-profile "$PROFILE" --wait
xcrun stapler staple "$DMG"
# Образ принимается, если он нотаризован: «source=Notarized Developer ID»
spctl --assess --type open --context context:primary-signature --verbose=2 "$DMG"
