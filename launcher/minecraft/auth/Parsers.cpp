#include "Parsers.h"
#include "Json.h"
#include "Logging.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>

namespace Parsers {

bool getDateTime(QJsonValue value, QDateTime& out)
{
    if (!value.isString()) {
        return false;
    }
    out = QDateTime::fromString(value.toString(), Qt::ISODate);
    return out.isValid();
}

bool getString(QJsonValue value, QString& out)
{
    if (!value.isString()) {
        return false;
    }
    out = value.toString();
    return true;
}

bool getNumber(QJsonValue value, double& out)
{
    if (!value.isDouble()) {
        return false;
    }
    out = value.toDouble();
    return true;
}

bool getNumber(QJsonValue value, int64_t& out)
{
    if (!value.isDouble()) {
        return false;
    }
    out = (int64_t)value.toDouble();
    return true;
}

bool getBool(QJsonValue value, bool& out)
{
    if (!value.isBool()) {
        return false;
    }
    out = value.toBool();
    return true;
}

/*
{
   "IssueInstant":"2020-12-07T19:52:08.4463796Z",
   "NotAfter":"2020-12-21T19:52:08.4463796Z",
   "Token":"token",
   "DisplayClaims":{
      "xui":[
         {
            "uhs":"userhash"
         }
      ]
   }
 }
*/
// TODO: handle error responses ...
/*
{
    "Identity":"0",
    "XErr":2148916238,
    "Message":"",
    "Redirect":"https://start.ui.xboxlive.com/AddChildToFamily"
}
// 2148916233 = missing XBox account
// 2148916238 = child account not linked to a family
*/

bool parseXTokenResponse(QByteArray& data, Token& output, QString name)
{
    qDebug() << "Parsing" << name << ":";
    qCDebug(authCredentials()) << data;
    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);
    if (jsonError.error) {
        qWarning() << "Failed to parse response from user.auth.xboxlive.com as JSON: " << jsonError.errorString();
        return false;
    }

    auto obj = doc.object();
    if (!getDateTime(obj.value("IssueInstant"), output.issueInstant)) {
        qWarning() << "User IssueInstant is not a timestamp";
        return false;
    }
    if (!getDateTime(obj.value("NotAfter"), output.notAfter)) {
        qWarning() << "User NotAfter is not a timestamp";
        return false;
    }
    if (!getString(obj.value("Token"), output.token)) {
        qWarning() << "User Token is not a string";
        return false;
    }
    auto arrayVal = obj.value("DisplayClaims").toObject().value("xui");
    if (!arrayVal.isArray()) {
        qWarning() << "Missing xui claims array";
        return false;
    }
    bool foundUHS = false;
    for (auto item : arrayVal.toArray()) {
        if (!item.isObject()) {
            continue;
        }
        auto obj_ = item.toObject();
        if (obj_.contains("uhs")) {
            foundUHS = true;
        } else {
            continue;
        }
        // consume all 'display claims' ... whatever that means
        for (auto iter = obj_.begin(); iter != obj_.end(); iter++) {
            QString claim;
            if (!getString(obj_.value(iter.key()), claim)) {
                qWarning() << "display claim " << iter.key() << " is not a string...";
                return false;
            }
            output.extra[iter.key()] = claim;
        }

        break;
    }
    if (!foundUHS) {
        qWarning() << "Missing uhs";
        return false;
    }
    output.validity = Validity::Certain;
    qDebug() << name << "is valid.";
    return true;
}

bool parseMinecraftProfile(QByteArray& data, MinecraftProfile& output)
{
    qDebug() << "Parsing Minecraft profile...";
    qCDebug(authCredentials()) << data;

    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);
    if (jsonError.error) {
        qWarning() << "Failed to parse response from user.auth.xboxlive.com as JSON: " << jsonError.errorString();
        return false;
    }

    auto obj = doc.object();
    if (!getString(obj.value("id"), output.id)) {
        qWarning() << "Minecraft profile id is not a string";
        return false;
    }

    if (!getString(obj.value("name"), output.name)) {
        qWarning() << "Minecraft profile name is not a string";
        return false;
    }

    auto skinsArray = obj.value("skins").toArray();
    for (auto skin : skinsArray) {
        auto skinObj = skin.toObject();
        Skin skinOut;
        if (!getString(skinObj.value("id"), skinOut.id)) {
            continue;
        }
        QString state;
        if (!getString(skinObj.value("state"), state)) {
            continue;
        }
        if (state != "ACTIVE") {
            continue;
        }
        if (!getString(skinObj.value("url"), skinOut.url)) {
            continue;
        }
        skinOut.url.replace("http://textures.minecraft.net", "https://textures.minecraft.net");
        if (!getString(skinObj.value("variant"), skinOut.variant)) {
            continue;
        }
        // we deal with only the active skin
        output.skin = skinOut;
        break;
    }
    auto capesArray = obj.value("capes").toArray();

    QString currentCape;
    for (auto cape : capesArray) {
        auto capeObj = cape.toObject();
        Cape capeOut;
        if (!getString(capeObj.value("id"), capeOut.id)) {
            continue;
        }
        QString state;
        if (!getString(capeObj.value("state"), state)) {
            continue;
        }
        if (state == "ACTIVE") {
            currentCape = capeOut.id;
        }
        if (!getString(capeObj.value("url"), capeOut.url)) {
            continue;
        }
        if (!getString(capeObj.value("alias"), capeOut.alias)) {
            continue;
        }

        output.capes[capeOut.id] = capeOut;
    }
    output.currentCape = currentCape;
    output.validity = Validity::Certain;
    return true;
}

namespace {
// these skin URLs are for the MHF_Steve and MHF_Alex accounts (made by a Mojang employee)
// they are needed because the session server doesn't return skin urls for default skins
static const QString SKIN_URL_STEVE =
    "https://textures.minecraft.net/texture/1a4af718455d4aab528e7a61f86fa25e6a369d1768dcb13f7df319a713eb810b";
static const QString SKIN_URL_ALEX =
    "https://textures.minecraft.net/texture/83cee5ca6afcdb171285aa00e8049c297b2dbeba0efb8ff970a5677a1b644032";

bool isDefaultModelSteve(QString uuid)
{
    // need to calculate *Java* hashCode of UUID
    // if number is even, skin/model is steve, otherwise it is alex

    // just in case dashes are in the id
    uuid.remove('-');

    if (uuid.size() != 32) {
        return true;
    }

    // qulonglong is guaranteed to be 64 bits
    // we need to use unsigned numbers to guarantee truncation below
    qulonglong most = uuid.left(16).toULongLong(nullptr, 16);
    qulonglong least = uuid.right(16).toULongLong(nullptr, 16);
    qulonglong xored = most ^ least;
    return ((static_cast<quint32>(xored >> 32)) ^ static_cast<quint32>(xored)) % 2 == 0;
}
}  // namespace

/**
Uses session server for skin/cape lookup instead of profile,
because locked Mojang accounts cannot access profile endpoint
(https://api.minecraftservices.com/minecraft/profile/)

ref: https://wiki.vg/Mojang_API#UUID_to_Profile_and_Skin.2FCape

{
    "id": "<profile identifier>",
    "name": "<player name>",
    "properties": [
        {
            "name": "textures",
            "value": "<base64 string>"
        }
    ]
}

decoded base64 "value":
{
    "timestamp": <java time in ms>,
    "profileId": "<profile uuid>",
    "profileName": "<player name>",
    "textures": {
        "SKIN": {
            "url": "<player skin URL>"
        },
        "CAPE": {
            "url": "<player cape URL>"
        }
    }
}
*/

bool parseMinecraftProfileMojang(QByteArray& data, MinecraftProfile& output)
{
    qDebug() << "Parsing Minecraft profile...";
    qCDebug(authCredentials()) << data;

    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);
    if (jsonError.error) {
        qWarning() << "Failed to parse response as JSON: " << jsonError.errorString();
        return false;
    }

    auto obj = Json::requireObject(doc, "mojang minecraft profile");
    if (!getString(obj.value("id"), output.id)) {
        qWarning() << "Minecraft profile id is not a string";
        return false;
    }

    if (!getString(obj.value("name"), output.name)) {
        qWarning() << "Minecraft profile name is not a string";
        return false;
    }

    auto propsArray = obj.value("properties").toArray();
    QByteArray texturePayload;
    for (auto p : propsArray) {
        auto pObj = p.toObject();
        auto name = pObj.value("name");
        if (!name.isString() || name.toString() != "textures") {
            continue;
        }

        auto value = pObj.value("value");
        if (value.isString()) {
            texturePayload = QByteArray::fromBase64(value.toString().toUtf8(), QByteArray::AbortOnBase64DecodingErrors);
        }

        if (!texturePayload.isEmpty()) {
            break;
        }
    }

    if (texturePayload.isNull()) {
        qWarning() << "No texture payload data";
        return false;
    }

    doc = QJsonDocument::fromJson(texturePayload, &jsonError);
    if (jsonError.error) {
        qWarning() << "Failed to parse response as JSON: " << jsonError.errorString();
        return false;
    }

    obj = Json::requireObject(doc, "session texture payload");
    auto textures = obj.value("textures");
    if (!textures.isObject()) {
        qWarning() << "No textures array in response";
        return false;
    }

    Skin skinOut;
    // fill in default skin info ourselves, as this endpoint doesn't provide it
    bool steve = isDefaultModelSteve(output.id);
    skinOut.variant = steve ? "CLASSIC" : "SLIM";
    skinOut.url = steve ? SKIN_URL_STEVE : SKIN_URL_ALEX;
    // sadly we can't figure this out, but I don't think it really matters...
    skinOut.id = "00000000-0000-0000-0000-000000000000";
    Cape capeOut;
    auto tObj = textures.toObject();
    for (auto idx = tObj.constBegin(); idx != tObj.constEnd(); ++idx) {
        if (idx->isObject()) {
            if (idx.key() == "SKIN") {
                auto skin = idx->toObject();
                if (!getString(skin.value("url"), skinOut.url)) {
                    qWarning() << "Skin url is not a string";
                    return false;
                }

                auto maybeMeta = skin.find("metadata");
                if (maybeMeta != skin.end() && maybeMeta->isObject()) {
                    auto meta = maybeMeta->toObject();
                    // might not be present
                    getString(meta.value("model"), skinOut.variant);
                }
            } else if (idx.key() == "CAPE") {
                auto cape = idx->toObject();
                if (!getString(cape.value("url"), capeOut.url)) {
                    qWarning() << "Cape url is not a string";
                    return false;
                }

                // we don't know the cape ID as it is not returned from the session server
                // so just fake it - changing capes is probably locked anyway :(
                capeOut.alias = "cape";
            }
        }
    }

    output.skin = skinOut;
    if (capeOut.alias == "cape") {
        output.capes = QMap<QString, Cape>({ { capeOut.alias, capeOut } });
        output.currentCape = capeOut.alias;
    }

    output.validity = Validity::Certain;
    return true;
}

bool parseMinecraftEntitlements(QByteArray& data, MinecraftEntitlement& output)
{
    qDebug() << "Parsing Minecraft entitlements...";
    qCDebug(authCredentials()) << data;

    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);
    if (jsonError.error) {
        qWarning() << "Failed to parse response from user.auth.xboxlive.com as JSON: " << jsonError.errorString();
        return false;
    }

    auto obj = doc.object();
    output.canPlayMinecraft = false;
    output.ownsMinecraft = false;

    auto itemsArray = obj.value("items").toArray();
    for (auto item : itemsArray) {
        auto itemObj = item.toObject();
        QString name;
        if (!getString(itemObj.value("name"), name)) {
            continue;
        }
        if (name == "game_minecraft") {
            output.canPlayMinecraft = true;
        }
        if (name == "product_minecraft") {
            output.ownsMinecraft = true;
        }
    }
    output.validity = Validity::Certain;
    return true;
}

bool parseRolloutResponse(QByteArray& data, bool& result)
{
    qDebug() << "Parsing Rollout response...";
    qCDebug(authCredentials()) << data;

    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);
    if (jsonError.error) {
        qWarning() << "Failed to parse response from https://api.minecraftservices.com/rollout/v1/msamigration as JSON: "
                   << jsonError.errorString();
        return false;
    }

    auto obj = doc.object();
    QString feature;
    if (!getString(obj.value("feature"), feature)) {
        qWarning() << "Rollout feature is not a string";
        return false;
    }
    if (feature != "msamigration") {
        qWarning() << "Rollout feature is not what we expected (msamigration), but is instead \"" << feature << "\"";
        return false;
    }
    if (!getBool(obj.value("rollout"), result)) {
        qWarning() << "Rollout feature is not a string";
        return false;
    }
    return true;
}

bool parseMojangResponse(QByteArray& data, Token& output)
{
    QJsonParseError jsonError;
    qDebug() << "Parsing Mojang response...";
    qCDebug(authCredentials()) << data;
    QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);
    if (jsonError.error) {
        qWarning() << "Failed to parse response from api.minecraftservices.com/launcher/login as JSON: " << jsonError.errorString();
        return false;
    }

    auto obj = doc.object();
    double expires_in = 0;
    if (!getNumber(obj.value("expires_in"), expires_in)) {
        qWarning() << "expires_in is not a valid number";
        return false;
    }
    auto currentTime = QDateTime::currentDateTimeUtc();
    output.issueInstant = currentTime;
    output.notAfter = currentTime.addSecs(expires_in);

    QString username;
    if (!getString(obj.value("username"), username)) {
        qWarning() << "username is not valid";
        return false;
    }

    // TODO: it's a JWT... validate it?
    if (!getString(obj.value("access_token"), output.token)) {
        qWarning() << "access_token is not valid";
        return false;
    }
    output.validity = Validity::Certain;
    qDebug() << "Mojang response is valid.";
    return true;
}

}  // namespace Parsers
