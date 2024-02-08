using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;

namespace updater.utils
{
    internal class Hash
    {
        static public string Evaluate(ZipArchiveEntry entry)
        {
            using (var entryStream = entry.Open())
            {
                using (var sha1 = SHA1.Create())
                {
                    // Format: 'blob {content length}\0{content}'
                    string header = $"blob {entry.Length}\0";
                    byte[] headerBytes = Encoding.ASCII.GetBytes(header);

                    sha1.TransformBlock(headerBytes, 0, headerBytes.Length, headerBytes, 0);

                    byte[] entryBytes = new byte[entry.Length];
                    entryStream.Read(entryBytes, 0, entryBytes.Length);

                    sha1.TransformFinalBlock(entryBytes, 0, entryBytes.Length);

                    return BitConverter.ToString(sha1.Hash).Replace("-", "").ToLower();
                }
            }
        }

        static public string Evaluate(string data)
        {
            using (SHA1 sha1 = SHA1.Create())
            {
                byte[] hashBytes = sha1.ComputeHash(Encoding.UTF8.GetBytes(data));
                return BitConverter.ToString(hashBytes).Replace("-", String.Empty);
            }
        }
        public static string EvaluatePath(string filePath)
        {
            using (var fileStream = new FileStream(filePath, FileMode.Open, FileAccess.Read))
            {
                using (var sha1 = SHA1.Create())
                {
                    // Format: 'blob {content length}\0{content}'
                    string header = $"blob {fileStream.Length}\0";
                    byte[] headerBytes = Encoding.ASCII.GetBytes(header);

                    sha1.TransformBlock(headerBytes, 0, headerBytes.Length, headerBytes, 0);

                    byte[] fileBytes = new byte[fileStream.Length];
                    fileStream.Read(fileBytes, 0, fileBytes.Length);

                    sha1.TransformFinalBlock(fileBytes, 0, fileBytes.Length);

                    return BitConverter.ToString(sha1.Hash).Replace("-", "").ToLower();
                }
            }
        }
    }
}
