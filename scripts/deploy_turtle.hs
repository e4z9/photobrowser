{- stack
  script
  --resolver lts-15.0
  --package turtle
  --package text
  --package string-combinators
-}

-- Run with "stack deploy_turtle.hs -q <qt_path> -t <target_app_bundle>"

{-# LANGUAGE OverloadedStrings #-}

import Turtle
import qualified Data.Text as T
import Data.String.Combinators (unlines)
import qualified System.IO as T
import Prelude hiding (FilePath, unlines)

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

argparser :: Parser Args
argparser = Args <$> (dir <$> optText "qt-path" 'q' "Qt root path")
                 <*> (dir <$> optText "target" 't' "Target application bundle")

-- |Forces the specified text to end with a slash and returns a corresponding 'FilePath'
-- 'FilePath's ending in a slash are interpreted by Turtle as a directory.
dir :: Text -> FilePath
dir = fromText . withSlash
    where withSlash p = if "/" `T.isSuffixOf` p
                           then p
                           else p <> "/"

-- |'qt.conf' file in target bundle.
qtConfFile bundle = bundle </> "Contents" </> "Resources" </> "qt.conf"

-- |Returns a list of plugin file paths to deploy from a Qt installation path
qtPluginFiles :: FilePath -> Shell FilePath
qtPluginFiles qt = do
    p <- select $ (qt </> "plugins" </>) <$> qtPluginPathNames
    find (suffix ".dylib") p

-- |Returns a list of framework directories to deploy from a Qt installation path
qtFrameworks :: FilePath -> [FilePath]
qtFrameworks qt = (\f -> qt </> "lib" </> (fromText f <.> "framework")) <$> qtFrameworkNames

-- |Collects the files to copy from a single framework.
frameworkFiles :: FilePath -> Shell FilePath
frameworkFiles qt = do
    f <- find (invert (headerPath <|> prl)) qt
    isDir <- testdir f
    if isDir then mempty else return f
    where headerPath = contains "/Headers/"
          prl = suffix ".prl"

-- |Collects the files to copy from all frameworks from a Qt installation path.
allFrameworkFiles :: FilePath -> Shell FilePath
allFrameworkFiles qt = select (qtFrameworks qt) >>= frameworkFiles

-- |Copies a file from a source directory to a destination directory.
-- The first argument is the copy action to perform, for example 'cp' or 'cptree'.
-- The following arguments are the source directory, destination directory, and the
-- full path to the file or directory to copy. The relative location of the file
-- to the source directory is kept in the destination directory.
-- Creates the destination directory if needed.
cpWithBase1 :: (FilePath -> FilePath -> Shell ()) ->
               FilePath -> FilePath -> FilePath -> Shell ()
cpWithBase1 f srcBase destBase fp = do
    let maybePart = stripPrefix srcBase fp
    sequence_ $ do
        part <- maybePart
        return $ do
            let fp' = destBase </> part
            mktree $ directory fp'
            f fp fp'

-- |Similar to 'cpWithBase1' but for multiple 'FilePath's with the same
-- source and destination directories.
cpWithBase :: (FilePath -> FilePath -> Shell ()) ->
              FilePath -> FilePath -> Shell FilePath -> Shell ()
cpWithBase f srcBase destBase files = files >>= cpWithBase1 f srcBase destBase

-- |Prints source and target FilePath before the specified action.
logged :: MonadIO io => (FilePath -> FilePath -> io ()) -> FilePath -> FilePath -> io ()
logged op f f' = do
    liftIO $ putStrLn $ encodeString f ++ " --> " ++ encodeString f'
    op f f'

main :: IO ()
main = do
    args <- options "Deploy tool" argparser
    let qt = _qtpath args
    let target = _bundlepath args
    let pluginsSrc = qt </> "plugins/"
    let frameworksSrc = qt </> "lib/"
    let pluginsTarget = target </> "Contents" </> "PlugIns"
    let frameworksTarget = target </> "Contents" </> "Frameworks"
    putStrLn $ "Qt:     " ++ encodeString qt
    putStrLn $ "Target: " ++ encodeString target
    putStrLn "-----------------------------"
    putStrLn "Copying Qt plugins"
    putStrLn "-----------------------------"
    sh $ cpWithBase (logged cp) pluginsSrc pluginsTarget (qtPluginFiles qt)
    putStrLn "-----------------------------"
    putStrLn "Copying Qt frameworks"
    putStrLn "-----------------------------"
    sh $ cpWithBase (logged cp) frameworksSrc frameworksTarget (allFrameworkFiles qt)
    let qtConf = qtConfFile target
    putStrLn "-----------------------------"
    putStrLn "Writing qt.conf"
    putStrLn "-----------------------------"
    putStrLn $ encodeString qtConf
    runManaged $ do
        liftIO $ mktree $ directory qtConf
        f <- writeonly qtConf
        liftIO $ T.hPutStr f qtConfig
