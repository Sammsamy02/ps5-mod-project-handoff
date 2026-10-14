// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Payload.IO;

/// <summary>
/// FreeBSD directory entry as returned by <see cref="PayloadFileSystem.readdir"/>.
/// Layout matches the FreeBSD 11.4 <c>struct dirent</c> (264 bytes, no d_off).
/// </summary>
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public unsafe struct FreeBsdDirent
{
    /// <summary>Inode number of the entry.</summary>
    public uint d_fileno;

    /// <summary>Length of this record.</summary>
    public ushort d_reclen;

    /// <summary>File type (DT_DIR=4, DT_REG=8, etc.).</summary>
    public byte d_type;

    /// <summary>Length of <see cref="d_name"/>.</summary>
    public byte d_namlen;

    /// <summary>Entry name (NUL-terminated, up to 255 characters).</summary>
    public fixed byte d_name[256];
}

/// <summary>
/// FreeBSD <c>struct stat</c> for the <c>stat(2)</c> system call. Layout matches the FreeBSD
/// 11.4 kernel's <c>struct stat</c> (120 bytes, not the Linux or FreeBSD 12.x layout).
/// </summary>
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct FreeBsdStat
{
    /// <summary>Device containing file.</summary>
    public uint st_dev;

    /// <summary>Inode number.</summary>
    public uint st_ino;

    /// <summary>File mode and permissions.</summary>
    public ushort st_mode;

    /// <summary>Number of hard links.</summary>
    public ushort st_nlink;

    /// <summary>User ID of owner.</summary>
    public uint st_uid;

    /// <summary>Group ID of owner.</summary>
    public uint st_gid;

    /// <summary>Device ID (if special file).</summary>
    public uint st_rdev;

    /// <summary>Access time (seconds).</summary>
    public long st_atim_sec;

    /// <summary>Access time (nanoseconds).</summary>
    public long st_atim_nsec;

    /// <summary>Modification time (seconds).</summary>
    public long st_mtim_sec;

    /// <summary>Modification time (nanoseconds).</summary>
    public long st_mtim_nsec;

    /// <summary>Status change time (seconds).</summary>
    public long st_ctim_sec;

    /// <summary>Status change time (nanoseconds).</summary>
    public long st_ctim_nsec;

    /// <summary>File size in bytes.</summary>
    public long st_size;

    /// <summary>Blocks allocated for file.</summary>
    public long st_blocks;

    /// <summary>Optimal I/O block size.</summary>
    public int st_blksize;

    /// <summary>User-defined flags.</summary>
    public uint st_flags;

    /// <summary>Generation number.</summary>
    public uint st_gen;

    /// <summary>Spare field.</summary>
    private int st_lspare;

    /// <summary>Birth time (seconds).</summary>
    public long st_birthtim_sec;

    /// <summary>Birth time (nanoseconds).</summary>
    public long st_birthtim_nsec;
}

/// <summary>
/// POSIX directory and file operations for a payload context. Wraps <c>opendir</c>,
/// <c>readdir</c>, <c>closedir</c>, and <c>stat</c> from <c>libc</c> for recursively
/// enumerating the filesystem after escaping the jail.
/// </summary>
public static unsafe partial class PayloadFileSystem
{
    private const string Lib = "libc";
    private const string LibKernel = "libkernel";
    private const string LibKernelSys = "libkernel_sys";

    /// <summary>File type constant: directory.</summary>
    public const ushort S_IFDIR = 0x4000;

    /// <summary>File type mask.</summary>
    public const ushort S_IFMT = 0xF000;

    /// <summary>Directory entry type: directory.</summary>
    public const byte DT_DIR = 4;

    /// <summary>Directory entry type: regular file.</summary>
    public const byte DT_REG = 8;

    /// <summary>Opens a directory for reading.</summary>
    /// <param name="name">A NUL-terminated UTF-8 path.</param>
    /// <returns>A directory handle, or null on error.</returns>
    [LibraryImport(Lib)]
    public static partial void* opendir(byte* name);

    /// <summary>Reads the next entry from a directory.</summary>
    /// <param name="dirp">A directory handle from <see cref="opendir"/>.</param>
    /// <returns>A pointer to the next <see cref="FreeBsdDirent"/>, or null at the end.</returns>
    [LibraryImport(Lib)]
    public static partial FreeBsdDirent* readdir(void* dirp);

    /// <summary>Closes a directory handle.</summary>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(Lib)]
    public static partial int closedir(void* dirp);

    /// <summary>Reads file status.</summary>
    /// <param name="path">A NUL-terminated UTF-8 path.</param>
    /// <param name="buf">A buffer to receive the status.</param>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int stat(byte* path, FreeBsdStat* buf);

    /// <summary>
    /// Returns <c>true</c> when <paramref name="mode"/> indicates a directory.
    /// Matches the <c>S_ISDIR</c> macro from <c>&lt;sys/stat.h&gt;</c>.
    /// </summary>
    public static bool IsDirectory(ushort mode) => (mode & S_IFMT) == S_IFDIR;

    // ---- Additional POSIX operations ----

    /// <summary>File exists.</summary>
    public const int F_OK = 0;

    /// <summary>Execute/search permission.</summary>
    public const int X_OK = 1;

    /// <summary>Write permission.</summary>
    public const int W_OK = 2;

    /// <summary>Read permission.</summary>
    public const int R_OK = 4;

    /// <summary>Creates a directory at <paramref name="path"/> with <paramref name="mode"/> permissions.</summary>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int mkdir(byte* path, ushort mode);

    /// <summary>Checks accessibility of <paramref name="path"/> against <paramref name="mode"/>
    /// (<see cref="F_OK"/>, <see cref="R_OK"/>, <see cref="W_OK"/>, <see cref="X_OK"/>).</summary>
    /// <returns>Zero when accessible, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int access(byte* path, int mode);

    /// <summary>Writes the current working directory into <paramref name="buf"/>, up to
    /// <paramref name="size"/> bytes.</summary>
    /// <returns>A pointer to <paramref name="buf"/> on success, or null on error.</returns>
    [LibraryImport(Lib)]
    public static partial byte* getcwd(byte* buf, nuint size);

    /// <summary>Renames (moves) a file or directory from <paramref name="from"/> to
    /// <paramref name="to"/>.</summary>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int rename(byte* from, byte* to);

    /// <summary>Removes a directory entry (file).</summary>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int unlink(byte* path);

    /// <summary>Removes an empty directory.</summary>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int rmdir(byte* path);

    /// <summary>
    /// Reads file status like <see cref="stat"/>, but does not follow symbolic links. When
    /// <paramref name="path"/> names a symlink, the returned status describes the link itself
    /// rather than its target.
    /// </summary>
    /// <param name="path">A NUL-terminated UTF-8 path.</param>
    /// <param name="buf">A buffer to receive the status.</param>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int lstat(byte* path, FreeBsdStat* buf);

    /// <summary>
    /// Truncates or extends the file referenced by <paramref name="fd"/> to
    /// <paramref name="length"/> bytes.
    /// </summary>
    /// <param name="fd">An open file descriptor with write permission.</param>
    /// <param name="length">The desired file size in bytes.</param>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int ftruncate(int fd, long length);

    /// <summary>Directory entry type: symbolic link.</summary>
    public const byte DT_LNK = 10;

    /// <summary>File type constant: symbolic link.</summary>
    public const ushort S_IFLNK = 0xA000;

    /// <summary>
    /// Returns <c>true</c> when <paramref name="mode"/> indicates a symbolic link.
    /// Matches the <c>S_ISLNK</c> macro from <c>&lt;sys/stat.h&gt;</c>.
    /// </summary>
    public static bool IsSymlink(ushort mode) => (mode & S_IFMT) == S_IFLNK;

    /// <summary>Changes the permission bits of the file at <paramref name="path"/>.</summary>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int chmod(byte* path, ushort mode);

    /// <summary>Open flag: read only.</summary>
    public const int O_RDONLY = 0;

    /// <summary>Open flag: write only.</summary>
    public const int O_WRONLY = 1;

    /// <summary>Open flag: read and write.</summary>
    public const int O_RDWR = 2;

    /// <summary>Open flag: create if it does not exist.</summary>
    public const int O_CREAT = 0x0200;

    /// <summary>Open flag: truncate to zero length.</summary>
    public const int O_TRUNC = 0x0400;

    /// <summary>
    /// Copies a single file from <paramref name="src"/> to <paramref name="dst"/>. Creates
    /// the destination, overwrites if it already exists.
    /// </summary>
    /// <returns><see langword="true"/> on success.</returns>
    public static bool CopyFile(byte* src, byte* dst)
    {
        int srcFd = PayloadIo.open(src, O_RDONLY);
        if (srcFd < 0) return false;

        int dstFd = PayloadIo.open(dst, O_WRONLY | O_CREAT | O_TRUNC);
        if (dstFd < 0)
        {
            PayloadIo.close(srcFd);
            return false;
        }

        byte* buf = stackalloc byte[8192];
        bool ok = true;
        while (true)
        {
            long n = PayloadIo.read(srcFd, buf, (nuint)8192);
            if (n <= 0) break;
            long written = 0;
            while (written < n)
            {
                long w = PayloadIo.write(dstFd, buf + written, (nuint)(n - written));
                if (w <= 0) { ok = false; break; }
                written += w;
            }
            if (!ok) break;
        }

        PayloadIo.close(dstFd);
        PayloadIo.close(srcFd);
        return ok;
    }

    /// <summary>
    /// Recursively copies the directory at <paramref name="src"/> to <paramref name="dst"/>.
    /// Creates the destination directory if it does not exist.
    /// </summary>
    /// <returns><see langword="true"/> when all files were copied successfully.</returns>
    public static bool CopyDirectory(byte* src, byte* dst)
    {
        mkdir(dst, 0x1FF); // 0777

        void* dir = opendir(src);
        if (dir == null) return false;

        byte* srcPath = stackalloc byte[1024];
        byte* dstPath = stackalloc byte[1024];
        bool ok = true;
        while (true)
        {
            FreeBsdDirent* entry = readdir(dir);
            if (entry == null) break;

            if (entry->d_name[0] == (byte)'.' &&
                (entry->d_name[1] == 0 ||
                 (entry->d_name[1] == (byte)'.' && entry->d_name[2] == 0)))
                continue;

            JoinPath(srcPath, src, entry->d_name);
            JoinPath(dstPath, dst, entry->d_name);

            if (entry->d_type == DT_DIR)
            {
                if (!CopyDirectory(srcPath, dstPath)) ok = false;
            }
            else
            {
                if (!CopyFile(srcPath, dstPath)) ok = false;
            }
        }

        closedir(dir);
        return ok;
    }

    /// <summary>
    /// Recursively removes the directory at <paramref name="path"/> and all its contents.
    /// </summary>
    /// <returns><see langword="true"/> when the directory was fully removed.</returns>
    public static bool RemoveDirectory(byte* path)
    {
        void* dir = opendir(path);
        if (dir == null) return false;

        byte* childPath = stackalloc byte[1024];
        bool ok = true;
        while (true)
        {
            FreeBsdDirent* entry = readdir(dir);
            if (entry == null) break;

            if (entry->d_name[0] == (byte)'.' &&
                (entry->d_name[1] == 0 ||
                 (entry->d_name[1] == (byte)'.' && entry->d_name[2] == 0)))
                continue;

            JoinPath(childPath, path, entry->d_name);

            if (entry->d_type == DT_DIR)
            {
                if (!RemoveDirectory(childPath)) ok = false;
            }
            else
            {
                if (unlink(childPath) != 0) ok = false;
            }
        }

        closedir(dir);
        if (rmdir(path) != 0) ok = false;
        return ok;
    }

    /// <summary>Changes the ownership of a file by path.</summary>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int chown(byte* path, uint owner, uint group);

    /// <summary>Changes the permissions of an open file descriptor.</summary>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int fchmod(int fd, ushort mode);

    /// <summary>Changes the ownership of an open file descriptor.</summary>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int fchown(int fd, uint owner, uint group);

    /// <summary>Truncates a file to <paramref name="length"/> bytes by path.</summary>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int truncate(byte* path, long length);

    /// <summary>Creates a hard link from <paramref name="name1"/> to <paramref name="name2"/>.</summary>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int link(byte* name1, byte* name2);

    /// <summary>Creates a named pipe (FIFO) at <paramref name="path"/>.</summary>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int mkfifo(byte* path, ushort mode);

    /// <summary>
    /// Sets the access and modification times of a file.
    /// </summary>
    /// <param name="path">A NUL-terminated UTF-8 path.</param>
    /// <param name="times">An array of two <c>timeval</c> structs (access time, modification
    /// time), each 16 bytes (tv_sec long + tv_usec long), or null to set to the current time.</param>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int utimes(byte* path, void* times);

    /// <summary>
    /// Reads the target of a symbolic link.
    /// </summary>
    [LibraryImport(LibKernelSys)]
    public static partial long readlink(byte* pathname, byte* buf, nuint bufsiz);

    /// <summary>
    /// Changes the working directory of the calling process.
    /// </summary>
    [LibraryImport(LibKernel)]
    public static partial int chdir(byte* path);

    private static void JoinPath(byte* dst, byte* dir, byte* name)
    {
        int i = 0;
        while (*dir != 0) { dst[i++] = *dir; dir++; }
        dst[i++] = (byte)'/';
        while (*name != 0) { dst[i++] = *name; name++; }
        dst[i] = 0;
    }
}
