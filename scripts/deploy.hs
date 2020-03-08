{- stack
  script
  --resolver lts-15.0
  --package shelly
  --package optparse-applicative
  --package filepath
  --package directory
  --package text
-}

-- Run with "stack deploy_shelly.hs -q <qt_path> -t <target_app_bundle>"

{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE ExtendedDefaultRules #-}

module Main where

import Options.Applicative
import Shelly hiding ((<.>))
import ShellyExt
import System.FilePath hiding ((</>))
import Control.Monad (filterM)
import Data.Foldable (traverse_)
import qualified Data.Text as T
import qualified Data.List as L
import Data.Maybe (maybeToList)

-- |Filter that specifies non-Qt libraries to deploy.
libsFilter lib = "/usr/local/" `L.isPrefixOf` lib

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

-- |Target for frameworks and libraries in bundle.
frameworksTarget bundle = bundle </> "Contents" </> "Frameworks"

-- |Target for plugins in bundle.
pluginsTarget bundle = bundle </> "Contents" </> "PlugIns"

-- |Executable file within an app bundle.
executablePath f = f </> "Contents" </> "MacOS" </> (takeBaseName . dropTrailingPathSeparator) f

-- |Collects the list of plugin files to deploy from a Qt installation path.
qtPluginFiles :: FilePath -> Sh [FilePath]
qtPluginFiles qt = do
    let dirs = (qt </> "plugins" </>) <$> qtPluginPathNames
    concat <$> findWhen isLibrary `traverse` dirs
    where isLibrary fp = return $ ".dylib" `L.isSuffixOf` fp

-- |Returns a list of framework directories to deploy from a Qt installation path.
qtFrameworks :: FilePath -> [FilePath]
qtFrameworks qt = (\f -> qt </> "lib" </> f <.> "framework") <$> qtFrameworkNames

-- |Collects the files to copy from a single framework.
-- Excludes headers and .prl files.
frameworkFiles :: FilePath -> Sh [FilePath]
frameworkFiles = findDirFilterWhen notHeaderPath (\fp -> notDir fp <&&> fileOk fp)
    where notHeaderPath fp = return $ not $ "/Headers" `L.isSuffixOf` fp
          notDir fp = not <$> test_d fp
          fileOk fp = return $ not $ ".prl" `L.isSuffixOf` fp
          (<&&>) = liftA2 (&&)

-- |Collects the files to copy from all frameworks from a Qt installation path.
allFrameworkFiles :: FilePath -> Sh [FilePath]
allFrameworkFiles qt = concat <$> frameworkFiles `traverse` qtFrameworks qt

-- |Deploys the specified Qt into the specified app bundle.
-- Logs the progress.
deployQt :: FilePath -- ^ Path to Qt installation
         -> FilePath -- ^ Path to app bundle
         -> IO ()
deployQt qt target = do
    let pluginsSrc = qt </> "plugins/"
    let frameworksSrc = qt </> "lib/"
    putStrLn $ "Qt:     " ++ qt
    putStrLn $ "Target: " ++ target
    putHr
    putStrLn "Copying Qt plugins"
    putHr
    shelly $ qtPluginFiles qt >>= cpWithBase (logged cp) pluginsSrc (pluginsTarget target)
    putHr
    putStrLn "Copying Qt frameworks"
    putHr
    shelly $ allFrameworkFiles qt >>= cpWithBase (logged cpPlusKeep) frameworksSrc (frameworksTarget target)
    putHr
    putStrLn "Writing qt.conf"
    putHr
    let qtConf = qtConfFile target
    putStrLn qtConf
    shelly $ do
        mkdir_p $ takeDirectory qtConf
        writefile qtConf $ T.pack qtConfig

-- |The 'BinInfo' type represents an executable or library together with dependencies.
-- Equality and ordering of 'BinInfo's is purely based on the path to the binary and ignores
-- the list of dependencies.
data BinInfo = BinInfo
    { getBinPath :: FilePath
    , getDependencies :: [FilePath] }

instance Eq BinInfo where
    (==) a b = getBinPath a == getBinPath b

instance Ord BinInfo where
    (<=) a b = getBinPath a <= getBinPath b

-- |Runs otool -L and reads the lines (after the first line) to extract all library references
binInfo :: FilePath -> Sh BinInfo
binInfo f = do
    output <- silently $ run "otool" ["-L", T.pack f]
    return $ BinInfo f (parse $ T.lines output)
    where parse [] = []
          parse (_:ls) = T.unpack <$> filter (not . T.null) (strip <$> ls)
          strip = T.strip . T.takeWhile (/= '(')

-- |Takes a list of binaries (libraries and executables) and recursively collects all dependencies.
-- The result also includes the 'BinInfo's of the specified binaries themselves.
-- Note that the result can contain semantic duplicates if the same library is referred to via
-- different symbolic links.
collectDependencies :: (FilePath -> Bool) -- ^ Only recurses into dependencies that match this predicate
                    -> [FilePath]         -- ^ List of binaries to start from
                    -> Sh [BinInfo]
collectDependencies predicate = go []
    where go :: [BinInfo] -> [FilePath] -> Sh [BinInfo]
          go acc []     = return acc
          go acc (f:fs) = do
                            info <- binInfo f
                            let acc' = info:acc
                            let add = filter predicate (getDependencies info)
                            let fs' = fs `L.union` (add L.\\ (getBinPath <$> acc'))
                            go acc' fs'

-- |Changes the references to the specified libraries to just "@rpath/libfilename.ext".
-- This ignores the actual paths to the binaries, just checks if the filename matches.
chpaths :: [FilePath] -- ^ List of references to change (as required)
        -> BinInfo    -- ^ 'BinInfo' for the binary to change
        -> Sh ()
chpaths pathsToChange lib = chpath lib `traverse_` pathsToChange
    where chpath lib path = maybe (pure ()) (chpath' lib) (findPath path lib)
          chpath' lib path = silently $ run_ "install_name_tool"
                                             (T.pack <$> ["-change", path,
                                                          "@rpath" </> takeFileName path, getBinPath lib])
          findPath path lib = L.find (takeFileName path `L.isSuffixOf`) (getDependencies lib)

-- |Adds a "@loader_path" relative RPATH from a binary to a destination directory.
addrpath :: FilePath -- ^ Destination directory
         -> FilePath -- ^ Binary to adapt
         -> Sh ()
addrpath destination fp = do
    let relDest = makeRelativeEx (takeDirectory fp) destination
    let rpath = "@loader_path" </> relDest
    silently $ run_ "install_name_tool" ["-add_rpath", T.pack rpath, T.pack fp]

-- |Copies a library to target path and changes all references to a specified list of libraries.
-- Logs the copy operation.
deployLib :: [FilePath] -- ^ List of library references to change, see 'chpaths'
          -> FilePath   -- ^ Target directory
          -> BinInfo    -- ^ Library to copy
          -> Sh ()
deployLib pathsToChange target lib = do
    mkdir_p target
    targetFilePath <- logged cpPlusFollow (getBinPath lib) target
    chpaths pathsToChange (BinInfo targetFilePath (getDependencies lib))

-- |The 'DeploymentInfo' type contains a list of files to be copied to a target directory.
-- Logs the progress.
data DeploymentInfo = DeploymentInfo
    { getTargetPath :: FilePath
    , getFiles :: [FilePath] }

-- |Deploys a list of binaries to the specified bundle, including dependencies.
deployLibs :: (FilePath -> Bool) -- ^ Filter to apply on the dependencies, to avoid deploying system libraries
           -> FilePath           -- ^ Target app bundle
           -> [DeploymentInfo]   -- ^ List of files to deploy
           -> Sh ()
deployLibs filter bundle infos = do
    let frameworksPath = frameworksTarget bundle
    echoHr
    echo "Copying libraries"
    echoHr
    -- create target directories
    (mkdir_p . getTargetPath) `traverse_` infos
    -- copy files from infos
    let copyInfo i = flip (logged cpPlusFollow) (getTargetPath i) `traverse` getFiles i
    deployedFiles <- concat <$> copyInfo `traverse` infos
    echoHr
    echo "Deploying dependencies"
    echoHr
    allBinInfos <- collectDependencies filter deployedFiles
    let (origInfos, depInfos) = L.partition (flip L.elem deployedFiles . getBinPath) allBinInfos
        addCanonic p = canonic p >>= (return . (:[p]))
    allBinaries <- concat <$> traverse (addCanonic . getBinPath) allBinInfos
    deployLib allBinaries frameworksPath `traverse_` depInfos
    echoHr
    echo "Fixing references"
    echoHr
    chpaths allBinaries `traverse_` origInfos
    addrpath frameworksPath `traverse_` deployedFiles

-- |Searches for "gst-plugin-scanner".
findGstPluginScanner :: Sh (Maybe FilePath)
findGstPluginScanner = findFold checkForScanner Nothing "/usr/local/Cellar/gstreamer"
    where checkForScanner (Just fp) _ = pure (Just fp)
          checkForScanner _ fp = if "/gst-plugin-scanner" `L.isSuffixOf` fp then pure (Just fp)
                                                                            else pure Nothing

-- |Collects all Gstreamer files to be deployed.
collectGstreamer :: FilePath -> Sh [DeploymentInfo]
collectGstreamer target = do
    echoHr
    echo "Scanning for Gstreamer tools"
    echoHr
    pluginFiles <- ls "/usr/local/lib/gstreamer-1.0" >>= filterM test_f >>= traverse canonic
    maybeScanner <- findGstPluginScanner
    let maybeScannerInfo = DeploymentInfo (target </> "Contents" </> "MacOS") . pure <$> maybeScanner
    return $ DeploymentInfo (pluginsTarget target </> "gstreamer-1.0") pluginFiles :
             maybeToList maybeScannerInfo

-- |Deploys Qt and Gstreamer
main :: IO ()
main = do
    args <- execParser argsInfo
    let qt = _qtpath args
    let target = _bundlepath args
    deployQt qt target
    gstreamerInfos <- shelly $ collectGstreamer target
    let exe = executablePath target
    let exeInfo = DeploymentInfo (takeDirectory exe) [exe]
    shelly $ deployLibs libsFilter target (exeInfo : gstreamerInfos)
