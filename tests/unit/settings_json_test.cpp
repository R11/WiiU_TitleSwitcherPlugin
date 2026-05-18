#include <gtest/gtest.h>

#include "storage/settings.h"
#include "storage/settings_json.h"

#include <cstring>

namespace {

Settings::PluginSettings makeSampleSettings()
{
    Settings::PluginSettings s;
    s.configVersion = 2;
    s.lastIndex = 17;
    s.lastCategoryIndex = 3;
    s.showNumbers = true;
    s.showFavorites = false;
    s.layoutPrefs.fontScale = 120;
    s.layoutPrefs.listWidthPercent = 25;
    s.layoutPrefs.iconSizePercent = 110;
    s.bgColor = 0x1E1E2EFF;
    s.titleColor = 0xFFFFFFFF;
    s.highlightedTitleColor = 0x89B4FAFF;
    s.favoriteColor = 0xF9E2AFFF;
    s.headerColor = 0xA6E3A1FF;
    s.categoryColor = 0xF5C2E7FF;
    s.nextCategoryId = 5;

    s.favorites.push_back(0x0005000010100000ULL);
    s.favorites.push_back(0x0005000010101000ULL);

    Settings::Category c1;
    c1.id = 1; c1.order = 0; c1.hidden = false;
    strncpy(c1.name, "RPG", Settings::MAX_CATEGORY_NAME);
    s.categories.push_back(c1);

    Settings::Category c2;
    c2.id = 2; c2.order = 1; c2.hidden = true;
    strncpy(c2.name, "Action", Settings::MAX_CATEGORY_NAME);
    s.categories.push_back(c2);

    Settings::TitleCategoryAssignment a1;
    a1.titleId = 0x0005000010100000ULL;
    a1.categoryId = 1;
    s.titleCategories.push_back(a1);

    return s;
}

} // namespace

TEST(SettingsJsonTest, RoundTripPreservesScalars)
{
    Settings::PluginSettings original = makeSampleSettings();
    std::string json = Settings::Json::Serialize(original);

    Settings::PluginSettings loaded;
    ASSERT_TRUE(Settings::Json::Deserialize(json, loaded));

    EXPECT_EQ(original.configVersion, loaded.configVersion);
    EXPECT_EQ(original.lastIndex, loaded.lastIndex);
    EXPECT_EQ(original.lastCategoryIndex, loaded.lastCategoryIndex);
    EXPECT_EQ(original.showNumbers, loaded.showNumbers);
    EXPECT_EQ(original.showFavorites, loaded.showFavorites);
    EXPECT_EQ(original.layoutPrefs.fontScale, loaded.layoutPrefs.fontScale);
    EXPECT_EQ(original.layoutPrefs.listWidthPercent, loaded.layoutPrefs.listWidthPercent);
    EXPECT_EQ(original.layoutPrefs.iconSizePercent, loaded.layoutPrefs.iconSizePercent);
    EXPECT_EQ(original.bgColor, loaded.bgColor);
    EXPECT_EQ(original.titleColor, loaded.titleColor);
    EXPECT_EQ(original.highlightedTitleColor, loaded.highlightedTitleColor);
    EXPECT_EQ(original.favoriteColor, loaded.favoriteColor);
    EXPECT_EQ(original.headerColor, loaded.headerColor);
    EXPECT_EQ(original.categoryColor, loaded.categoryColor);
    EXPECT_EQ(original.nextCategoryId, loaded.nextCategoryId);
}

TEST(SettingsJsonTest, RoundTripPreservesFavorites)
{
    Settings::PluginSettings original = makeSampleSettings();
    std::string json = Settings::Json::Serialize(original);

    Settings::PluginSettings loaded;
    ASSERT_TRUE(Settings::Json::Deserialize(json, loaded));

    ASSERT_EQ(original.favorites.size(), loaded.favorites.size());
    for (size_t i = 0; i < original.favorites.size(); ++i) {
        EXPECT_EQ(original.favorites[i], loaded.favorites[i]);
    }
}

TEST(SettingsJsonTest, RoundTripPreservesCategories)
{
    Settings::PluginSettings original = makeSampleSettings();
    std::string json = Settings::Json::Serialize(original);

    Settings::PluginSettings loaded;
    ASSERT_TRUE(Settings::Json::Deserialize(json, loaded));

    ASSERT_EQ(original.categories.size(), loaded.categories.size());
    for (size_t i = 0; i < original.categories.size(); ++i) {
        EXPECT_EQ(original.categories[i].id, loaded.categories[i].id);
        EXPECT_EQ(original.categories[i].order, loaded.categories[i].order);
        EXPECT_EQ(original.categories[i].hidden, loaded.categories[i].hidden);
        EXPECT_STREQ(original.categories[i].name, loaded.categories[i].name);
    }
}

TEST(SettingsJsonTest, RoundTripPreservesTitleCategories)
{
    Settings::PluginSettings original = makeSampleSettings();
    std::string json = Settings::Json::Serialize(original);

    Settings::PluginSettings loaded;
    ASSERT_TRUE(Settings::Json::Deserialize(json, loaded));

    ASSERT_EQ(original.titleCategories.size(), loaded.titleCategories.size());
    for (size_t i = 0; i < original.titleCategories.size(); ++i) {
        EXPECT_EQ(original.titleCategories[i].titleId, loaded.titleCategories[i].titleId);
        EXPECT_EQ(original.titleCategories[i].categoryId, loaded.titleCategories[i].categoryId);
    }
}

TEST(SettingsJsonTest, DefaultRoundTrip)
{
    Settings::PluginSettings original;
    std::string json = Settings::Json::Serialize(original);

    Settings::PluginSettings loaded;
    ASSERT_TRUE(Settings::Json::Deserialize(json, loaded));

    EXPECT_EQ(original.configVersion, loaded.configVersion);
    EXPECT_EQ(original.lastIndex, loaded.lastIndex);
    EXPECT_EQ(original.bgColor, loaded.bgColor);
    EXPECT_EQ(original.favorites.size(), loaded.favorites.size());
    EXPECT_EQ(original.categories.size(), loaded.categories.size());
}

TEST(SettingsJsonTest, EmptyInputReturnsFalse)
{
    Settings::PluginSettings loaded;
    EXPECT_FALSE(Settings::Json::Deserialize("", loaded));
}

TEST(SettingsJsonTest, IgnoresUnknownKeys)
{
    const char* json = R"({"lastIndex": 5, "someFutureField": 42, "showNumbers": true})";
    Settings::PluginSettings loaded;
    ASSERT_TRUE(Settings::Json::Deserialize(json, loaded));
    EXPECT_EQ(5, loaded.lastIndex);
    EXPECT_TRUE(loaded.showNumbers);
}
