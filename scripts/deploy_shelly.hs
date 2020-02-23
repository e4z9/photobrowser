{- stack
  script
  --resolver lts-15.0
  --package shelly
  --package optparse-applicative
  --package filepath
  --package text
-}

-- Run with "stack deploy_shelly.hs -q <qt_path> -t <target_app_bundle>"

{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE ExtendedDefaultRules #-}

import Options.Applicative
import Data.Semigroup ((<>))
import Shelly hiding ((<.>))
import System.FilePath (takeDirectory,(<.>))
import qualified Data.Text as T
import Data.List (isSuffixOf)
import Control.Monad.IO.Class

-- |Qt plugins to copy.
qtPluginPathNames = ["iconengines", "imageformats", "platforms", "styles"]

-- |Qt frameworks to copy.
qtFrameworkNames = ["QtCore", "QtGui", "QtWidgets", "QtDBus", "QtPrintSupport"]

-- |Content of target 'qt.conf' file.
qtConfig = unlines [
    "[Paths]",
    "Binaries = MacOS",
    "Plugins = PlugIns",
    "Libraries = Frameworks"
    ]

data Args = Args
    { _qtpath :: FilePath
    , _bundlepath :: FilePath } deriving Show

argsParser :: Parser Args
argsParser = Args
     <$> strOption
         ( long "qt-path" <> short 'q' <> metavar "QT_PATH"
         <> help "Path to Qt installation" )
     <*> strOption
         ( long "target" <> short 't' <> metavar "APP_BUNDLE"
         <> help "Path to target app bundle" )

argsInfo :: ParserInfo Args
argsInfo = info (argsParser <**> helper) (briefDesc <> progDesc "Deployment tool")

-- |'qt.conf' file in target bundle.
qtConfFile bundle = bundle </> "Contents" </> "Resources" </> "qt.conf"

-- |Collects the list of plugin file paths to deploy from a Qt installation path.
qtPluginFiles :: FilePath -> Sh [FilePath]
qtPluginFiles qt = do
    let dirs = (qt </> "plugins" </>) <$> qtPluginPathNames
    concat <$> findWhen isLibrary `mapM` dirs
    where isLibrary fp = return $ ".dylib" `isSuffixOf` fp

-- |Returns a list of framework directories to deploy from a Qt installation path.
qtFrameworks :: FilePath -> [FilePath]
qtFrameworks qt = (\f -> qt </> "lib" </> f <.> "framework") <$> qtFrameworkNames

-- |Collects the files to copy from a single framework.
frameworkFiles :: FilePath -> Sh [FilePath]
frameworkFiles = findDirFilterWhen notHeaderPath (\fp -> (&&) <$> notDir fp <*> fileOk fp)
    where notHeaderPath fp = return $ not $ "/Headers" `isSuffixOf` fp
          notDir fp = not <$> test_d fp
          fileOk fp = return $ not $ ".prl" `isSuffixOf` fp

-- |Collects the files to copy from all frameworks from a Qt installation path.
allFrameworkFiles :: FilePath -> Sh [FilePath]
allFrameworkFiles qt = concat <$> frameworkFiles `mapM` qtFrameworks qt

-- |Copies a file from a source directory to a destination directory.
-- The first argument is the copy action to perform, for example 'cp' or 'cp_r'.
-- The following arguments are the source directory, destination directory, and the
-- full path to the file or directory to copy. The relative location of the file
-- to the source directory is kept in the destination directory.
-- Creates the destination directory if needed.
cpWithBase1 :: (FilePath -> FilePath -> Sh ()) ->
               FilePath -> FilePath -> FilePath -> Sh ()
cpWithBase1 f srcBase destBase fp = do
    fpPart <- relativeTo srcBase fp
    let fp' = destBase </> fpPart
    mkdir_p $ takeDirectory fp'
    f fp fp'

-- |Similar to 'cpWithBase1' but for multiple 'FilePath's with the same
-- source and destination directories.
cpWithBase :: (FilePath -> FilePath -> Sh ()) ->
              FilePath -> FilePath -> [FilePath] -> Sh ()
cpWithBase f srcBase destBase files = sequence_ $ cpWithBase1 f srcBase destBase <$> files

-- |Prints source and target FilePath before the specified action.
logged :: MonadIO io => (FilePath -> FilePath -> io ()) -> FilePath -> FilePath -> io ()
logged op f f' = do
    liftIO $ putStrLn $ f ++ " --> " ++ f'
    op f f'

deployQt :: FilePath -> FilePath -> IO ()
deployQt qt target = do
    let pluginsSrc = qt </> "plugins/"
    let frameworksSrc = qt </> "lib/"
    let pluginsTarget = target </> "Contents" </> "PlugIns"
    let frameworksTarget = target </> "Contents" </> "Frameworks"
    putStrLn $ "Qt:     " ++ qt
    putStrLn $ "Target: " ++ target
    putStrLn "-----------------------------"
    putStrLn "Copying Qt plugins"
    putStrLn "-----------------------------"
    shelly $ qtPluginFiles qt >>= cpWithBase (logged cp) pluginsSrc pluginsTarget
    putStrLn "-----------------------------"
    putStrLn "Copying Qt frameworks"
    putStrLn "-----------------------------"
    shelly $ allFrameworkFiles qt >>= cpWithBase (logged cp) frameworksSrc frameworksTarget
    putStrLn "-----------------------------"
    putStrLn "Writing qt.conf"
    putStrLn "-----------------------------"
    let qtConf = qtConfFile target
    putStrLn qtConf
    shelly $ do
        mkdir_p $ takeDirectory qtConf
        writefile qtConf $ T.pack qtConfig

main :: IO ()
main = do
    args <- execParser argsInfo
    let qt = _qtpath args
    let target = _bundlepath args
    deployQt qt target
