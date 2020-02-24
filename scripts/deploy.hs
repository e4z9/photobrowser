{- stack
  script
  --resolver lts-15.0
  --package shelly
  --package optparse-applicative
  --package filepath
  --package directory
  --package text
  --package containers
-}

-- Run with "stack deploy_shelly.hs -q <qt_path> -t <target_app_bundle>"

{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE ExtendedDefaultRules #-}

module Main where

import Options.Applicative
import Data.Semigroup ((<>))
import Shelly hiding ((<.>))
import System.Directory (getPermissions, setPermissions, setOwnerWritable)
import System.FilePath hiding ((</>))
import Control.Monad (filterM)
import qualified Data.Text as T
import qualified Data.List as L
import Data.Maybe (maybeToList)
import qualified Data.Set as S
import Control.Monad.IO.Class

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
    concat <$> findWhen isLibrary `mapM` dirs
    where isLibrary fp = return $ ".dylib" `L.isSuffixOf` fp

-- |Returns a list of framework directories to deploy from a Qt installation path.
qtFrameworks :: FilePath -> [FilePath]
qtFrameworks qt = (\f -> qt </> "lib" </> f <.> "framework") <$> qtFrameworkNames

-- |Collects the files to copy from a single framework.
-- Excludes headers and .prl files.
frameworkFiles :: FilePath -> Sh [FilePath]
frameworkFiles = findDirFilterWhen notHeaderPath (\fp -> (&&) <$> notDir fp <*> fileOk fp)
    where notHeaderPath fp = return $ not $ "/Headers" `L.isSuffixOf` fp
          notDir fp = not <$> test_d fp
          fileOk fp = return $ not $ ".prl" `L.isSuffixOf` fp

-- |Collects the files to copy from all frameworks from a Qt installation path.
allFrameworkFiles :: FilePath -> Sh [FilePath]
allFrameworkFiles qt = concat <$> frameworkFiles `mapM` qtFrameworks qt

-- |Copies a file from a source directory to a destination directory.
-- The first argument is the copy action to perform, for example 'cp' or 'cp_r'.
-- The following arguments are the source directory, destination directory, and the
-- full path to the file or directory to copy. The relative location of the file
-- to the source directory is kept in the destination directory.
-- Creates the destination directory if needed.
cpWithBase1 :: (FilePath -> FilePath -> Sh ()) -- ^ Copy operation to perform
            -> FilePath                        -- ^ Source base path
            -> FilePath                        -- ^ Destination base path
            -> FilePath                        -- ^ File in the source path to copy to destination
            -> Sh ()
cpWithBase1 f srcBase destBase fp = do
    fpPart <- relativeTo srcBase fp
    let fp' = destBase </> fpPart
    mkdir_p $ takeDirectory fp'
    f fp fp'

-- |Similar to 'cpWithBase1' but for multiple 'FilePath's with the same
-- source and destination directories.
cpWithBase :: (FilePath -> FilePath -> Sh ()) -- ^ Copy operation to perform
            -> FilePath                       -- ^ Source base path
            -> FilePath                       -- ^ Destination base path
            -> [FilePath]                     -- ^ Files in the source path to copy to destination
            -> Sh ()
cpWithBase f srcBase destBase files = cpWithBase1 f srcBase destBase `mapM_` files

-- |Prints source and target FilePath before the specified action.
logged :: MonadIO io
       => (FilePath -> FilePath -> io a) -- ^ Action to perform on source and destination
       -> FilePath                       -- ^ Source
       -> FilePath                       -- ^ Destination
       -> io a
logged op f f' = do
    liftIO $ putStrLn $ f ++ " --> " ++ f'
    op f f'

putHr = putStrLn "-----------------------------"
echoHr :: MonadIO io => io ()
echoHr = liftIO putHr

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
    shelly $ allFrameworkFiles qt >>= cpWithBase (logged cp) frameworksSrc (frameworksTarget target)
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
collectDependencies predicate binaries = S.toList <$> _run S.empty (S.fromList binaries)
    where _run :: S.Set BinInfo -> S.Set FilePath -> Sh (S.Set BinInfo)
          _run acc fs
              | null fs   = return acc
              | otherwise = do
                                let next = S.elemAt 0 fs
                                info <- binInfo next
                                let acc' = S.insert info acc
                                let add = S.fromList $ filter predicate (getDependencies info)
                                let fs' = S.deleteAt 0 fs `S.union` S.difference add (getBinPath `S.map` acc')
                                _run acc' fs'

-- |Changes the references to the specified libraries to just "@rpath/libfilename.ext".
-- This ignores the actual paths to the binaries, just checks if the filename matches.
chpaths :: [FilePath] -- ^ List of references to change (as required)
        -> BinInfo    -- ^ 'BinInfo' for the binary to change
        -> Sh ()
chpaths pathsToChange lib = chpath lib `mapM_` pathsToChange
    where chpath lib path = maybe (pure ()) (chpath' lib) (findPath path lib)
          chpath' lib path = silently $ run_ "install_name_tool"
                                             (T.pack <$> ["-change", path,
                                                          "@rpath" </> takeFileName path, getBinPath lib])
          findPath path lib = L.find (takeFileName path `L.isSuffixOf`) (getDependencies lib)

-- |Returns a relative path from the source to the target. This is purely text based.
-- If the paths do not have a common prefix, they are assumed to be located in the same root directory.
makeRelativeEx :: FilePath -> FilePath -> FilePath
makeRelativeEx from to = joinPath (dots ++ to')
    where (from', to') = stripCommonPrefix (splitPath from) (splitPath to)
          stripCommonPrefix [] bs                 = ([], bs)
          stripCommonPrefix as []                 = (as, [])
          stripCommonPrefix ass@(a:as) bss@(b:bs) = if a == b then stripCommonPrefix as bs
                                                              else (ass, bss)
          dots = ".." <$ from'

-- |Adds a "@loader_path" relative RPATH from a binary to a destination directory.
addrpath :: FilePath -- ^ Destination directory
         -> FilePath -- ^ Binary to adapt
         -> Sh ()
addrpath destination fp = do
    let relDest = makeRelativeEx (takeDirectory fp) destination
    let rpath = "@loader_path" </> relDest
    silently $ run_ "install_name_tool" ["-add_rpath", T.pack rpath, T.pack fp]

-- |Changes file permissions to owner-writable.
makeWritable :: FilePath -> IO ()
makeWritable f = do
    perms <- getPermissions f
    setPermissions f (setOwnerWritable True perms)

-- |Uses 'cp' to copy a file to a destination, creating the target directory if necessary, and
-- ensuring that the result is owner-writable.
cpWritable :: FilePath -- ^ Source file
           -> FilePath -- ^ Target directory
           -> Sh FilePath
cpWritable lib targetDir = do
    mkdir_p targetDir
    cp lib targetDir
    let targetFilePath = targetDir </> takeFileName lib
    liftIO $ makeWritable targetFilePath
    return targetFilePath

-- |Copies a library to target path and changes all references to a specified list of libraries.
-- Logs the copy operation.
deployLib :: [FilePath] -- ^ List of library references to change, see 'chpaths'
          -> FilePath   -- ^ Target directory
          -> BinInfo    -- ^ Library to copy
          -> Sh ()
deployLib pathsToChange target lib = do
    targetFilePath <- logged cpWritable (getBinPath lib) target
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
    let copyInfo i = flip (logged cpWritable) (getTargetPath i) `mapM` getFiles i
    deployedFiles <- concat <$> copyInfo `mapM` infos
    echoHr
    echo "Deploying dependencies"
    echoHr
    allBinInfos <- collectDependencies filter deployedFiles
    let (origInfos, depInfos) = L.partition (flip L.elem deployedFiles . getBinPath) allBinInfos
        addCanonic p = canonic p >>= (return . (:[p]))
    allBinaries <- concat <$> mapM (addCanonic . getBinPath) allBinInfos
    deployLib allBinaries frameworksPath `mapM_` depInfos
    echoHr
    echo "Fixing references"
    echoHr
    chpaths allBinaries `mapM_` origInfos
    addrpath frameworksPath `mapM_` deployedFiles

-- |Searches for "gst-plugin-scanner".
-- TOOD can this be made to bail out of the fold early?
findGstPluginScanner :: Sh (Maybe FilePath)
findGstPluginScanner = findFold (\fpy fp -> if "/gst-plugin-scanner" `L.isSuffixOf` fp
                                                then pure (Just fp)
                                                else pure fpy)
                                Nothing "/usr/local/Cellar/gstreamer"

-- |Collects all Gstreamer files to be deployed.
collectGstreamer :: FilePath -> Sh [DeploymentInfo]
collectGstreamer target = do
    echoHr
    echo "Scanning for Gstreamer tools"
    echoHr
    pluginFiles <- ls "/usr/local/lib/gstreamer-1.0" >>= filterM test_f >>= mapM canonic
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
