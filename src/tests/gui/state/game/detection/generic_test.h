/*  LOOT

A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
Fallout: New Vegas.

Copyright (C) 2014 WrinklyNinja

This file is part of LOOT.

LOOT is free software: you can redistribute
it and/or modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

LOOT is distributed in the hope that it will
be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with LOOT.  If not, see
<https://www.gnu.org/licenses/>.
*/

#ifndef LOOT_TESTS_GUI_STATE_GAME_DETECTION_GENERIC_TEST
#define LOOT_TESTS_GUI_STATE_GAME_DETECTION_GENERIC_TEST

#include "gui/state/game/detection/generic.h"
#include "gui/state/game/detection/gog.h"
#include "tests/common_game_test_fixture.h"
#include "tests/gui/state/game/detection/test_registry.h"

namespace loot::test {
class Generic_FindGameInstallsTest
    : public CommonGameTestFixture,
      public testing::WithParamInterface<GameId> {
protected:
  Generic_FindGameInstallsTest() : CommonGameTestFixture(GetParam()) {}

  void SetUp() override {
    CommonGameTestFixture::SetUp();

    initialCurrentPath = std::filesystem::current_path();

    const auto gamePath = dataPath / "..";
    const auto lootPath = gamePath / "LOOT";

    // Change the current path into a game subfolder.
    std::filesystem::create_directory(lootPath);
    std::filesystem::current_path(lootPath);
  }

  void TearDown() override {
    // Restore the previous current path.
    RestoreCurrentPath();

    CommonGameTestFixture::TearDown();
  }

  void RestoreCurrentPath() const {
    std::filesystem::current_path(initialCurrentPath);
  }

  std::string GetSubKey() const {
    switch (GetParam()) {
      case GameId::tes3:
        return "Software\\Bethesda Softworks\\Morrowind";
      case GameId::tes4:
        return "Software\\Bethesda Softworks\\Oblivion";
      case GameId::nehrim:
        return "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Nehrim"
               " - At Fate's Edge_is1";
      case GameId::tes5:
        return "Software\\Bethesda Softworks\\Skyrim";
      case GameId::enderal:
        return "SOFTWARE\\SureAI\\Enderal";
      case GameId::tes5se:
        return "Software\\Bethesda Softworks\\Skyrim Special Edition";
      case GameId::enderalse:
        return "SOFTWARE\\SureAI\\EnderalSE";
      case GameId::tes5vr:
        return "Software\\Bethesda Softworks\\Skyrim VR";
      case GameId::fo3:
        return "Software\\Bethesda Softworks\\Fallout3";
      case GameId::fonv:
        return "Software\\Bethesda Softworks\\FalloutNV";
      case GameId::fo4:
        return "Software\\Bethesda Softworks\\Fallout4";
      case GameId::fo4vr:
        return "Software\\Bethesda Softworks\\Fallout 4 VR";
      default:
        throw std::logic_error("Unrecognised game ID");
    }
  }

  void CreateSteamFile() const {
    if (GetParam() == GameId::tes3) {
      touch(dataPath.parent_path() / "steam_autocloud.vdf");
    } else if (GetParam() == GameId::nehrim) {
      touch(dataPath.parent_path() / "steam_api.dll");
    } else {
      touch(dataPath.parent_path() / "installscript.vdf");
    }
  }

private:
  std::filesystem::path initialCurrentPath;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(,
                         Generic_FindGameInstallsTest,
                         ::testing::ValuesIn(ALL_GAME_IDS));

TEST_P(Generic_FindGameInstallsTest, shouldNotFindInvalidSiblingGame) {
  std::filesystem::remove_all(dataPath);

  const auto gameInstalls =
      loot::generic::FindGameInstalls(TestRegistry(), GetParam());

  EXPECT_TRUE(gameInstalls.empty());
}

TEST_P(Generic_FindGameInstallsTest, shouldFindGameInParentOfCurrentDirectory) {
  const auto gameId = GetParam();
  const auto gameInstalls =
      loot::generic::FindGameInstalls(TestRegistry(), gameId);

  auto expectedSource = InstallSource::unknown;
  if (GetParam() == GameId::tes5 || GetParam() == GameId::tes5vr ||
      GetParam() == GameId::fo4vr) {
    expectedSource = InstallSource::steam;
  }

  ASSERT_EQ(1, gameInstalls.size());
  EXPECT_EQ(gameId, gameInstalls[0].gameId);
  EXPECT_EQ(expectedSource, gameInstalls[0].source);
  EXPECT_EQ(std::filesystem::current_path().parent_path(),
            gameInstalls[0].installPath);
  EXPECT_EQ("", gameInstalls[0].localPath);
}

TEST_P(Generic_FindGameInstallsTest, shouldIdentifySteamSiblingGame) {
  CreateSteamFile();

  const auto gameId = GetParam();
  const auto gameInstalls =
      loot::generic::FindGameInstalls(TestRegistry(), gameId);

  ASSERT_EQ(1, gameInstalls.size());
  EXPECT_EQ(gameId, gameInstalls[0].gameId);
  EXPECT_EQ(InstallSource::steam, gameInstalls[0].source);
  EXPECT_EQ(std::filesystem::current_path().parent_path(),
            gameInstalls[0].installPath);
  EXPECT_EQ("", gameInstalls[0].localPath);
}

TEST_P(Generic_FindGameInstallsTest, shouldIdentifyGogSiblingGame) {
  const auto gogGameIds = gog::GetGogGameIds(GetParam());
  if (!gogGameIds.empty()) {
    touch(dataPath.parent_path() / ("goggame-" + gogGameIds[0] + ".ico"));
  }

  const auto gameId = GetParam();
  const auto gameInstalls =
      loot::generic::FindGameInstalls(TestRegistry(), gameId);

  auto expectedSource = InstallSource::gog;
  if (GetParam() == GameId::tes5 || GetParam() == GameId::tes5vr ||
      GetParam() == GameId::fo4vr) {
    expectedSource = InstallSource::steam;
  } else if (GetParam() == GameId::enderal || GetParam() == GameId::fo4) {
    expectedSource = InstallSource::unknown;
  }

  ASSERT_EQ(1, gameInstalls.size());
  EXPECT_EQ(gameId, gameInstalls[0].gameId);
  EXPECT_EQ(expectedSource, gameInstalls[0].source);
  EXPECT_EQ(std::filesystem::current_path().parent_path(),
            gameInstalls[0].installPath);
  EXPECT_EQ("", gameInstalls[0].localPath);
}

TEST_P(Generic_FindGameInstallsTest, shouldIdentifyEpicSiblingGame) {
  const auto gameId = GetParam();

  if (gameId == GameId::tes5se) {
    touch(dataPath.parent_path() / "EOSSDK-Win64-Shipping.dll");
  } else if (gameId == GameId::fo3) {
    touch(dataPath.parent_path() / "FalloutLauncherEpic.exe");
  } else if (gameId == GameId::fonv) {
    touch(dataPath.parent_path() / "EOSSDK-Win32-Shipping.dll");
  }

  const auto gameInstalls =
      loot::generic::FindGameInstalls(TestRegistry(), gameId);

  auto expectedSource = InstallSource::unknown;
  if (gameId == GameId::tes5 || gameId == GameId::tes5vr ||
      gameId == GameId::fo4vr) {
    expectedSource = InstallSource::steam;
  } else if (gameId == GameId::tes5se || gameId == GameId::fo3 ||
             gameId == GameId::fonv) {
    expectedSource = InstallSource::epic;
  }

  ASSERT_EQ(1, gameInstalls.size());
  EXPECT_EQ(gameId, gameInstalls[0].gameId);
  EXPECT_EQ(expectedSource, gameInstalls[0].source);
  EXPECT_EQ(std::filesystem::current_path().parent_path(),
            gameInstalls[0].installPath);
  EXPECT_EQ("", gameInstalls[0].localPath);
}

TEST_P(Generic_FindGameInstallsTest, shouldIdentifyMsStoreSiblingGame) {
  if (GetParam() == GameId::tes5se || GetParam() == GameId::fo4) {
    touch(dataPath.parent_path() / "appxmanifest.xml");
  } else {
    touch(dataPath.parent_path().parent_path() / "appxmanifest.xml");
  }

  const auto gameId = GetParam();
  const auto gameInstalls =
      loot::generic::FindGameInstalls(TestRegistry(), gameId);

  auto expectedSource = InstallSource::microsoft;
  if (GetParam() == GameId::tes5 || GetParam() == GameId::tes5vr ||
      GetParam() == GameId::fo4vr) {
    expectedSource = InstallSource::steam;
  } else if (GetParam() == GameId::nehrim || GetParam() == GameId::enderal ||
             GetParam() == GameId::enderalse) {
    expectedSource = InstallSource::unknown;
  }

  ASSERT_EQ(1, gameInstalls.size());
  EXPECT_EQ(gameId, gameInstalls[0].gameId);
  EXPECT_EQ(expectedSource, gameInstalls[0].source);
  EXPECT_EQ(std::filesystem::current_path().parent_path(),
            gameInstalls[0].installPath);
  EXPECT_EQ("", gameInstalls[0].localPath);
}

TEST_P(Generic_FindGameInstallsTest, shouldFindGameUsingGenericRegistryValue) {
  RestoreCurrentPath();

  TestRegistry registry;
  registry.SetStringValue(GetSubKey(), dataPath.parent_path().u8string());

  const auto gameInstalls =
      loot::generic::FindGameInstalls(registry, GetParam());

  auto expectedSource = InstallSource::unknown;
  if (GetParam() == GameId::tes5 || GetParam() == GameId::tes5vr ||
      GetParam() == GameId::fo4vr) {
    expectedSource = InstallSource::steam;
  }

  ASSERT_EQ(1, gameInstalls.size());
  EXPECT_EQ(GetParam(), gameInstalls[0].gameId);
  EXPECT_EQ(expectedSource, gameInstalls[0].source);
  EXPECT_EQ(dataPath.parent_path(), gameInstalls[0].installPath);
  EXPECT_EQ("", gameInstalls[0].localPath);
}

TEST_P(Generic_FindGameInstallsTest,
       shouldNotFindGameIfPathInRegistryIsInvalid) {
  RestoreCurrentPath();

  TestRegistry registry;
  registry.SetStringValue(GetSubKey(), "invalid");

  const auto gameInstalls =
      loot::generic::FindGameInstalls(registry, GetParam());

  EXPECT_TRUE(gameInstalls.empty());
}

TEST_P(Generic_FindGameInstallsTest, shouldIdentifySteamRegistryGame) {
  RestoreCurrentPath();
  CreateSteamFile();

  TestRegistry registry;
  registry.SetStringValue(GetSubKey(), dataPath.parent_path().u8string());

  const auto gameInstalls =
      loot::generic::FindGameInstalls(registry, GetParam());

  ASSERT_EQ(1, gameInstalls.size());
  EXPECT_EQ(GetParam(), gameInstalls[0].gameId);
  EXPECT_EQ(InstallSource::steam, gameInstalls[0].source);
  EXPECT_EQ(dataPath.parent_path(), gameInstalls[0].installPath);
  EXPECT_EQ("", gameInstalls[0].localPath);
}

TEST_P(Generic_FindGameInstallsTest, shouldIdentifyGogRegistryGame) {
  RestoreCurrentPath();

  const auto gogGameIds = gog::GetGogGameIds(GetParam());
  if (!gogGameIds.empty()) {
    touch(dataPath.parent_path() / ("goggame-" + gogGameIds[0] + ".ico"));
  }

  TestRegistry registry;
  registry.SetStringValue(GetSubKey(), dataPath.parent_path().u8string());

  const auto gameInstalls =
      loot::generic::FindGameInstalls(registry, GetParam());

  auto expectedSource = InstallSource::gog;
  if (GetParam() == GameId::tes5 || GetParam() == GameId::tes5vr ||
      GetParam() == GameId::fo4vr) {
    expectedSource = InstallSource::steam;
  } else if (GetParam() == GameId::enderal || GetParam() == GameId::fo4) {
    expectedSource = InstallSource::unknown;
  }

  ASSERT_EQ(1, gameInstalls.size());
  EXPECT_EQ(GetParam(), gameInstalls[0].gameId);
  EXPECT_EQ(expectedSource, gameInstalls[0].source);
  EXPECT_EQ(dataPath.parent_path(), gameInstalls[0].installPath);
  EXPECT_EQ("", gameInstalls[0].localPath);
}

class DetectGameInstallTest : public CommonGameTestFixture,
                              public testing::WithParamInterface<GameId> {
protected:
  DetectGameInstallTest() : CommonGameTestFixture(GetParam()) {
    if (GetParam() == GameId::nehrim) {
      touch(dataPath.parent_path() / "NehrimLauncher.exe");
    } else if (GetParam() == GameId::enderal ||
               GetParam() == GameId::enderalse) {
      touch(dataPath.parent_path() / "Enderal Launcher.exe");
    }
  }

  GameSettings GetSettings() const {
    GameSettings settings =
        GameSettings(getGameType()).SetGamePath(dataPath.parent_path());
    if (GetParam() == GameId::nehrim) {
      settings.SetMaster("Nehrim.esm");
    }

    return settings;
  }
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(,
                         DetectGameInstallTest,
                         ::testing::ValuesIn(ALL_GAME_IDS));

TEST_P(DetectGameInstallTest, shouldNotDetectAGameInstallThatIsNotValid) {
  const auto install = generic::DetectGameInstall(GameSettings(getGameType()));

  EXPECT_FALSE(install.has_value());
}

TEST_P(DetectGameInstallTest, shouldDetectAValidGameInstall) {
  const auto install = generic::DetectGameInstall(GetSettings());

  auto expectedSource = InstallSource::unknown;
  if (GetParam() == GameId::tes5 || GetParam() == GameId::tes5vr ||
      GetParam() == GameId::fo4vr) {
    expectedSource = InstallSource::steam;
  }

  ASSERT_TRUE(install.has_value());
  EXPECT_EQ(GetParam(), install.value().gameId);
  EXPECT_EQ(expectedSource, install.value().source);
  EXPECT_EQ(dataPath.parent_path(), install.value().installPath);
  EXPECT_EQ("", install.value().localPath);
}

TEST_P(DetectGameInstallTest, shouldDetectASteamInstall) {
  if (GetParam() == GameId::tes3) {
    touch(dataPath.parent_path() / "steam_autocloud.vdf");
  } else if (GetParam() == GameId::nehrim) {
    touch(dataPath.parent_path() / "steam_api.dll");
  } else {
    touch(dataPath.parent_path() / "installscript.vdf");
  }

  const auto install = generic::DetectGameInstall(GetSettings());

  ASSERT_TRUE(install.has_value());
  EXPECT_EQ(GetParam(), install.value().gameId);
  EXPECT_EQ(InstallSource::steam, install.value().source);
  EXPECT_EQ(dataPath.parent_path(), install.value().installPath);
  EXPECT_EQ("", install.value().localPath);
}

TEST_P(DetectGameInstallTest, shouldDetectAGogInstall) {
  const auto gogGameIds = gog::GetGogGameIds(GetParam());
  if (!gogGameIds.empty()) {
    touch(dataPath.parent_path() / ("goggame-" + gogGameIds[0] + ".ico"));
  }

  const auto install = generic::DetectGameInstall(GetSettings());

  auto expectedSource = InstallSource::gog;
  if (GetParam() == GameId::tes5 || GetParam() == GameId::tes5vr ||
      GetParam() == GameId::fo4vr) {
    expectedSource = InstallSource::steam;
  } else if (GetParam() == GameId::enderal || GetParam() == GameId::fo4) {
    expectedSource = InstallSource::unknown;
  }

  ASSERT_TRUE(install.has_value());
  EXPECT_EQ(GetParam(), install.value().gameId);
  EXPECT_EQ(expectedSource, install.value().source);
  EXPECT_EQ(dataPath.parent_path(), install.value().installPath);
  EXPECT_EQ("", install.value().localPath);
}

TEST_P(DetectGameInstallTest, shouldDetectAnEpicInstall) {
  if (GetParam() == GameId::tes5se) {
    touch(dataPath.parent_path() / "EOSSDK-Win64-Shipping.dll");
  } else if (GetParam() == GameId::fo3) {
    touch(dataPath.parent_path() / "FalloutLauncherEpic.exe");
  } else if (GetParam() == GameId::fonv) {
    touch(dataPath.parent_path() / "EOSSDK-Win32-Shipping.dll");
  }

  const auto install = generic::DetectGameInstall(GetSettings());

  auto expectedSource = InstallSource::unknown;
  if (GetParam() == GameId::tes5 || GetParam() == GameId::tes5vr ||
      GetParam() == GameId::fo4vr) {
    expectedSource = InstallSource::steam;
  } else if (GetParam() == GameId::tes5se || GetParam() == GameId::fo3 ||
             GetParam() == GameId::fonv) {
    expectedSource = InstallSource::epic;
  }

  ASSERT_TRUE(install.has_value());
  EXPECT_EQ(GetParam(), install.value().gameId);
  EXPECT_EQ(expectedSource, install.value().source);
  EXPECT_EQ(dataPath.parent_path(), install.value().installPath);
  EXPECT_EQ("", install.value().localPath);
}

TEST_P(DetectGameInstallTest, shouldDetectAMicrosoftInstall) {
  if (GetParam() == GameId::tes5se || GetParam() == GameId::fo4) {
    touch(dataPath.parent_path() / "appxmanifest.xml");
  } else {
    touch(dataPath.parent_path().parent_path() / "appxmanifest.xml");
  }

  const auto install = generic::DetectGameInstall(GetSettings());

  auto expectedSource = InstallSource::microsoft;
  if (GetParam() == GameId::tes5 || GetParam() == GameId::tes5vr ||
      GetParam() == GameId::fo4vr) {
    expectedSource = InstallSource::steam;
  } else if (GetParam() == GameId::nehrim || GetParam() == GameId::enderal ||
             GetParam() == GameId::enderalse) {
    expectedSource = InstallSource::unknown;
  }

  ASSERT_TRUE(install.has_value());
  EXPECT_EQ(GetParam(), install.value().gameId);
  EXPECT_EQ(expectedSource, install.value().source);
  EXPECT_EQ(dataPath.parent_path(), install.value().installPath);
  EXPECT_EQ("", install.value().localPath);
}
}

#endif
