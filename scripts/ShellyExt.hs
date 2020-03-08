module ShellyExt where

import Control.Monad.IO.Class
import Data.Foldable (traverse_)
import Shelly hiding ((<.>))
import System.Directory
import System.FilePath hiding ((</>))

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

-- |Copies a file from a source directory to a destination directory.
-- The first argument is the copy action to perform, for example 'cp' or 'cp_r'.
-- The following arguments are the source directory, destination directory, and the
-- full path to the file or directory to copy. The relative location of the file
-- to the source directory is kept in the destination directory.
-- Creates the destination directory if needed.
cpWithBase1 :: (FilePath -> FilePath -> Sh a) -- ^ Copy operation to perform
            -> FilePath                        -- ^ Source base path
            -> FilePath                        -- ^ Destination base path
            -> FilePath                        -- ^ File in the source path to copy to destination
            -> Sh a
cpWithBase1 f srcBase destBase fp = do
    fpPart <- relativeTo srcBase fp
    let fp' = destBase </> fpPart
    mkdir_p $ takeDirectory fp'
    f fp fp'

-- |Similar to 'cpWithBase1' but for multiple 'FilePath's with the same
-- source and destination directories.
cpWithBase :: (FilePath -> FilePath -> Sh a) -- ^ Copy operation to perform
            -> FilePath                       -- ^ Source base path
            -> FilePath                       -- ^ Destination base path
            -> [FilePath]                     -- ^ Files in the source path to copy to destination
            -> Sh ()
cpWithBase f srcBase destBase files = cpWithBase1 f srcBase destBase `traverse_` files

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

-- |Changes file permissions to owner-writable.
makeWritable :: FilePath -> IO ()
makeWritable f = do
    perms <- getPermissions f
    setPermissions f (setOwnerWritable True perms)

data FollowSymlinks = KeepSymlinks | FollowSymlinks

-- |Uses 'cp' to copy a file to a destination, ensuring that the result is owner-writable.
-- If the target is an existing directory, the file is copied to that directory, otherwise
-- the target is the new file path including file name. Ensures that the target directory
-- exists in the latter case.
cpPlus :: FollowSymlinks -- ^ Copy symlinks as symlinks or copy symlink target
       -> FilePath       -- ^ Source file
       -> FilePath       -- ^ Target file or directory
       -> Sh FilePath
cpPlus followSym src target = do
    targetIsDir <- test_d target
    let targetDir = if targetIsDir then target else takeDirectory target
    let targetFilePath = if targetIsDir then target </> takeFileName src else target
    unless targetIsDir $ mkdir_p targetDir
    let cp' = do cp src targetFilePath; liftIO $ makeWritable targetFilePath
        copyLink' = copyLink src targetFilePath
    case followSym of
        FollowSymlinks -> cp'
        KeepSymlinks   -> ifM (test_s src) copyLink' cp'
    return targetFilePath
    where ifM b t f = do b <- b; if b then t else f
          copyLink src targetFilePath = do
              linkTarget <- liftIO $ getSymbolicLinkTarget src
              ifM (test_d (takeDirectory src </> linkTarget))
                  (liftIO $ createDirectoryLink linkTarget targetFilePath)
                  (liftIO $ createFileLink linkTarget targetFilePath)

-- |Shortcut for `cpPlus` with `KeepSymlinks`.
cpPlusKeep = cpPlus KeepSymlinks
-- |Shortcut for `cpPlus` with `FollowSymlinks`.
cpPlusFollow = cpPlus FollowSymlinks
