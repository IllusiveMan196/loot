/*  LOOT

A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
Fallout: New Vegas.

Copyright (C) 2014-2016    WrinklyNinja

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

#ifndef LOOT_TESTS_BACKEND_GAME_GAME_TEST
#define LOOT_TESTS_BACKEND_GAME_GAME_TEST

#include "gui/state/game.h"

#include "loot/exception/game_detection_error.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace gui {
namespace test {
class GameTest : public loot::test::CommonGameTestFixture {
protected:
  GameTest() : loadOrderToSet_({
    masterFile,
    blankEsm,
    blankMasterDependentEsm,
    blankDifferentEsm,
    blankDifferentMasterDependentEsm,
    blankDifferentEsp,
    blankDifferentPluginDependentEsp,
    blankEsp,
    blankMasterDependentEsp,
    blankDifferentMasterDependentEsp,
    blankPluginDependentEsp,
  }),
  loadOrderBackupFile0("loadorder.bak.0"),
  loadOrderBackupFile1("loadorder.bak.1"),
  loadOrderBackupFile2("loadorder.bak.2"),
  loadOrderBackupFile3("loadorder.bak.3") {}

  void TearDown() {
    CommonGameTestFixture::TearDown();
  }

  std::vector<std::string> loadOrderToSet_;
  const std::string loadOrderBackupFile0;
  const std::string loadOrderBackupFile1;
  const std::string loadOrderBackupFile2;
  const std::string loadOrderBackupFile3;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_CASE_P(,
                        GameTest,
                        ::testing::Values(
                          GameType::tes4,
                          GameType::tes5,
                          GameType::fo3,
                          GameType::fonv,
                          GameType::fo4,
                          GameType::tes5se));

TEST_P(GameTest, constructingFromGameSettingsShouldUseTheirValues) {
  GameSettings settings = GameSettings(GetParam(), "folder");
  settings.SetName("foo");
  settings.SetMaster(blankEsm);
  settings.SetRegistryKey("foo");
  settings.SetRepoURL("foo");
  settings.SetRepoBranch("foo");
  settings.SetGamePath(localPath);
  Game game = Game(settings, lootDataPath, localPath);

  EXPECT_EQ(GetParam(), game.Type());
  EXPECT_EQ(settings.Name(), game.Name());
  EXPECT_EQ(settings.FolderName(), game.FolderName());
  EXPECT_EQ(settings.Master(), game.Master());
  EXPECT_EQ(settings.RegistryKey(), game.RegistryKey());
  EXPECT_EQ(settings.RepoURL(), game.RepoURL());
  EXPECT_EQ(settings.RepoBranch(), game.RepoBranch());

  EXPECT_EQ(settings.GamePath(), game.GamePath());
  EXPECT_EQ(lootDataPath / "folder" / "masterlist.yaml", game.MasterlistPath());
  EXPECT_EQ(lootDataPath / "folder" / "userlist.yaml", game.UserlistPath());
}

#ifndef _WIN32
// Testing on Windows will find real game installs in the Registry, so cannot
// test autodetection fully unless on Linux.
TEST_P(GameTest, constructingShouldThrowOnLinuxIfGamePathIsNotGiven) {
  EXPECT_THROW(Game(GameSettings(GetParam()), "", localPath), std::system_error);
}

TEST_P(GameTest, constructingShouldThrowOnLinuxIfLocalPathIsNotGiven) {
  auto settings = GameSettings(GetParam()).SetGamePath(dataPath.parent_path());
  EXPECT_THROW(Game(settings, lootDataPath), std::system_error);
}
#else
TEST_P(GameTest, constructingShouldNotThrowOnWindowsIfLocalPathIsNotGiven) {
  auto settings = GameSettings(GetParam()).SetGamePath(dataPath.parent_path());
  EXPECT_NO_THROW(Game(settings, lootDataPath, ""));
}
#endif

TEST_P(GameTest, isInstalledShouldBeFalseIfGamePathIsNotSet) {
  EXPECT_FALSE(Game::IsInstalled(GameSettings(GetParam())));
}

TEST_P(GameTest, isInstalledShouldBeTrueIfGamePathIsValid) {
  EXPECT_TRUE(Game::IsInstalled(GameSettings(GetParam()).SetGamePath(dataPath.parent_path())));
}

TEST_P(GameTest, initShouldNotCreateAGameFolderIfTheLootDataPathIsEmpty) {
  Game game = Game(GameSettings(GetParam()).SetGamePath(dataPath.parent_path()), "", localPath);

  ASSERT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName()));
  EXPECT_NO_THROW(game.Init());

  EXPECT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName()));
}

TEST_P(GameTest, initShouldCreateAGameFolderIfTheCreateFolderArgumentIsTrue) {
  Game game = Game(GameSettings(GetParam()).SetGamePath(dataPath.parent_path()), lootDataPath, localPath);

  ASSERT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName()));
  EXPECT_NO_THROW(game.Init());

  EXPECT_TRUE(boost::filesystem::exists(lootDataPath / game.FolderName()));
}

TEST_P(GameTest, initShouldNotThrowIfGameAndLocalPathsAreNotEmpty) {
  Game game = Game(GameSettings(GetParam()).SetGamePath(dataPath.parent_path()), "", localPath);

  EXPECT_NO_THROW(game.Init());
}

TEST_P(GameTest, redatePluginsShouldRedatePluginsForSkyrimAndSkyrimSEAndDoNothingForOtherGames) {
  Game game = Game(GameSettings(GetParam()).SetGamePath(dataPath.parent_path()), "", localPath);
  game.Init();

  std::vector<std::pair<std::string, bool>> loadOrder = getInitialLoadOrder();

  // First set reverse timestamps to be sure.
  time_t time = boost::filesystem::last_write_time(dataPath / masterFile);
  for (size_t i = 1; i < loadOrder.size(); ++i) {
    if (!boost::filesystem::exists(dataPath / loadOrder[i].first))
      loadOrder[i].first += ".ghost";

    boost::filesystem::last_write_time(dataPath / loadOrder[i].first, time - i * 60);
    ASSERT_EQ(time - i * 60, boost::filesystem::last_write_time(dataPath / loadOrder[i].first));
  }

  EXPECT_NO_THROW(game.RedatePlugins());

  time_t interval = 60;
  if (GetParam() != GameType::tes5 && GetParam() != GameType::tes5se)
    interval *= -1;

  for (size_t i = 0; i < loadOrder.size(); ++i) {
    EXPECT_EQ(time + i * interval, boost::filesystem::last_write_time(dataPath / loadOrder[i].first));
  }
}

TEST_P(GameTest, loadAllInstalledPluginsWithHeadersOnlyTrueShouldLoadTheHeadersOfAllInstalledPlugins) {
  Game game = Game(GameSettings(GetParam()).SetGamePath(dataPath.parent_path()), "", localPath);
  ASSERT_NO_THROW(game.Init());

  EXPECT_NO_THROW(game.LoadAllInstalledPlugins(true));
  EXPECT_EQ(11, game.GetPlugins().size());

  // Check that one plugin's header has been read.
  ASSERT_NO_THROW(game.GetPlugin(masterFile));
  auto plugin = game.GetPlugin(masterFile);
  EXPECT_EQ("5.0", plugin->GetVersion());

  // Check that only the header has been read.
  EXPECT_EQ(0, plugin->GetCRC());
}

TEST_P(GameTest, loadAllInstalledPluginsWithHeadersOnlyFalseShouldFullyLoadAllInstalledPlugins) {
  Game game = Game(GameSettings(GetParam()).SetGamePath(dataPath.parent_path()), "", localPath);
  ASSERT_NO_THROW(game.Init());

  EXPECT_NO_THROW(game.LoadAllInstalledPlugins(false));
  EXPECT_EQ(11, game.GetPlugins().size());

  // Check that one plugin's header has been read.
  ASSERT_NO_THROW(game.GetPlugin(blankEsm));
  auto plugin = game.GetPlugin(blankEsm);
  EXPECT_EQ("5.0", plugin->GetVersion());

  // Check that not only the header has been read.
  EXPECT_EQ(blankEsmCrc, plugin->GetCRC());
}

TEST_P(GameTest, pluginsShouldNotBeFullyLoadedByDefault) {
  Game game = Game(GameSettings(GetParam()).SetGamePath(dataPath.parent_path()), "", localPath);

  EXPECT_FALSE(game.ArePluginsFullyLoaded());
}

TEST_P(GameTest, pluginsShouldNotBeFullyLoadedAfterLoadingHeadersOnly) {
  Game game = Game(GameSettings(GetParam()).SetGamePath(dataPath.parent_path()), "", localPath);

  ASSERT_NO_THROW(game.LoadAllInstalledPlugins(true));

  EXPECT_FALSE(game.ArePluginsFullyLoaded());
}

TEST_P(GameTest, pluginsShouldBeFullyLoadedAfterFullyLoadingThem) {
  Game game = Game(GameSettings(GetParam()).SetGamePath(dataPath.parent_path()), "", localPath);

  ASSERT_NO_THROW(game.LoadAllInstalledPlugins(false));

  EXPECT_TRUE(game.ArePluginsFullyLoaded());
}

TEST_P(GameTest, GetActiveLoadOrderIndexShouldReturnNegativeOneForAPluginThatIsNotActive) {
  Game game = Game(GameSettings(GetParam()).SetGamePath(dataPath.parent_path()), "", localPath);
  game.Init();

  short index = game.GetActiveLoadOrderIndex(blankEsp);

  EXPECT_EQ(-1, index);
}

TEST_P(GameTest, GetActiveLoadOrderIndexShouldReturnTheLoadOrderIndexOmittingInactivePlugins) {
  Game game = Game(GameSettings(GetParam()).SetGamePath(dataPath.parent_path()), "", localPath);
  game.Init();

  short index = game.GetActiveLoadOrderIndex(masterFile);
  EXPECT_EQ(0, index);

  index = game.GetActiveLoadOrderIndex(blankEsm);
  EXPECT_EQ(1, index);

  index = game.GetActiveLoadOrderIndex(blankDifferentMasterDependentEsp);
  EXPECT_EQ(2, index);
}

TEST_P(GameTest, setLoadOrderShouldCreateABackupOfTheCurrentLoadOrder) {
  Game game = Game(GameSettings(GetParam()).SetGamePath(dataPath.parent_path()), lootDataPath, localPath);
  game.Init();

  ASSERT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile0));
  ASSERT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile1));
  ASSERT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile2));
  ASSERT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile3));

  auto initialLoadOrder = getLoadOrder();
  ASSERT_NO_THROW(game.SetLoadOrder(loadOrderToSet_));

  EXPECT_TRUE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile0));
  EXPECT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile1));
  EXPECT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile2));
  EXPECT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile3));

  auto loadOrder = readFileLines(lootDataPath / game.FolderName() / loadOrderBackupFile0);

  EXPECT_EQ(initialLoadOrder, loadOrder);
}

TEST_P(GameTest, setLoadOrderShouldRollOverExistingBackups) {
  Game game = Game(GameSettings(GetParam()).SetGamePath(dataPath.parent_path()), lootDataPath, localPath);
  game.Init();

  ASSERT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile0));
  ASSERT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile1));
  ASSERT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile2));
  ASSERT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile3));

  auto initialLoadOrder = getLoadOrder();
  ASSERT_NO_THROW(game.SetLoadOrder(loadOrderToSet_));

  auto firstSetLoadOrder = loadOrderToSet_;

  ASSERT_NE(blankPluginDependentEsp, loadOrderToSet_[9]);
  ASSERT_NE(blankDifferentMasterDependentEsp, loadOrderToSet_[10]);
  loadOrderToSet_[9] = blankPluginDependentEsp;
  loadOrderToSet_[10] = blankDifferentMasterDependentEsp;

  ASSERT_NO_THROW(game.SetLoadOrder(loadOrderToSet_));

  EXPECT_TRUE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile0));
  EXPECT_TRUE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile1));
  EXPECT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile2));
  EXPECT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile3));

  auto loadOrder = readFileLines(lootDataPath / game.FolderName() / loadOrderBackupFile0);
  EXPECT_EQ(firstSetLoadOrder, loadOrder);

  loadOrder = readFileLines(lootDataPath / game.FolderName() / loadOrderBackupFile1);
  EXPECT_EQ(initialLoadOrder, loadOrder);
}

TEST_P(GameTest, setLoadOrderShouldKeepUpToThreeBackups) {
  Game game = Game(GameSettings(GetParam()).SetGamePath(dataPath.parent_path()), lootDataPath, localPath);
  game.Init();

  ASSERT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile0));
  ASSERT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile1));
  ASSERT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile2));
  ASSERT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile3));

  auto initialLoadOrder = getLoadOrder();
  ASSERT_NO_THROW(game.SetLoadOrder(loadOrderToSet_));

  auto firstSetLoadOrder = loadOrderToSet_;

  ASSERT_NE(blankPluginDependentEsp, loadOrderToSet_[9]);
  ASSERT_NE(blankDifferentMasterDependentEsp, loadOrderToSet_[10]);
  loadOrderToSet_[9] = blankPluginDependentEsp;
  loadOrderToSet_[10] = blankDifferentMasterDependentEsp;

  ASSERT_NO_THROW(game.SetLoadOrder(loadOrderToSet_));

  auto secondSetLoadOrder = loadOrderToSet_;

  ASSERT_NE(blankMasterDependentEsp, loadOrderToSet_[7]);
  ASSERT_NE(blankEsp, loadOrderToSet_[8]);
  loadOrderToSet_[7] = blankMasterDependentEsp;
  loadOrderToSet_[8] = blankEsp;

  ASSERT_NO_THROW(game.SetLoadOrder(loadOrderToSet_));

  auto thirdSetLoadOrder = loadOrderToSet_;

  ASSERT_NE(blankDifferentMasterDependentEsm, loadOrderToSet_[3]);
  ASSERT_NE(blankDifferentEsm, loadOrderToSet_[4]);
  loadOrderToSet_[3] = blankDifferentMasterDependentEsm;
  loadOrderToSet_[4] = blankDifferentEsm;

  ASSERT_NO_THROW(game.SetLoadOrder(loadOrderToSet_));

  EXPECT_TRUE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile0));
  EXPECT_TRUE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile1));
  EXPECT_TRUE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile2));
  EXPECT_FALSE(boost::filesystem::exists(lootDataPath / game.FolderName() / loadOrderBackupFile3));

  auto loadOrder = readFileLines(lootDataPath / game.FolderName() / loadOrderBackupFile0);
  EXPECT_EQ(thirdSetLoadOrder, loadOrder);

  loadOrder = readFileLines(lootDataPath / game.FolderName() / loadOrderBackupFile1);
  EXPECT_EQ(secondSetLoadOrder, loadOrder);

  loadOrder = readFileLines(lootDataPath / game.FolderName() / loadOrderBackupFile2);
  EXPECT_EQ(firstSetLoadOrder, loadOrder);
}
}
}
}

#endif