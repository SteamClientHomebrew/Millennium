using KellermanSoftware.WhatsChanged;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Data;
using System.IO;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text;
using System.Windows.Forms;

namespace updater
{
    internal class Inspector
    {
        string FileUrl;
        string ApiUrl;
        string ZipArchiveName;
        string ThemeName;
        public Inspector(string fileUrl, string apiUrl)
        {
            string name = utils.Hash.Evaluate(fileUrl).Substring(0, 15);

            ZipArchiveName = Path.Combine(Directory.GetCurrentDirectory(), "steamui", "skins", name + ".zip");

            FileUrl = fileUrl;
            ApiUrl = apiUrl;
        }
        public bool Start()
        {
            Logger.Log($"[+] [PATH:{ZipArchiveName}]");
            Logger.Log($"[+] [FILE:{FileUrl}]");

            bool success = builder.DownloadFileAsync.Download(FileUrl, ZipArchiveName).Result;

            if (!success)
            {
                Logger.Log("Couldn't download update. aborting...");
                return false;
            }

            if (!this.InspectArchive())
            {
                return false;
            }

            try
            {
                Logger.Log("[+] Deleting downloaded archive...", true);
                File.Delete(ZipArchiveName);

                Logger.Log(" [good]", ConsoleColor.Green);
            }
            catch (Exception ex)
            {
                Logger.Log(" [bad]", ConsoleColor.Red);
                Logger.Log(ex.ToString());
            }
            return true;
        }

        public struct FileInformation
        {
            public string FileName;
            public string ShaHash;
            public FileInformation(string fileName, string shaHash)
            {
                FileName = fileName;
                ShaHash = shaHash;
            }
        }

        void PromptDiscardUpdate(ZipArchiveEntry entry, string extractedFilePath)
        {
            string message = $"A portion of ['{ThemeName}'] received an update, but it appears it has manually been edited. "
                + "Not updating will result in your theme being behind in updates, however updating it will result in a loss of the manually edited data."
                + $"\n\nAffected file:\n{extractedFilePath}\n\nDo you want to overwrite this file? [If you don't remember edited this file choose yes]";

            DialogResult result = MessageBox.Show(message, "Millennium - Potential Conflict", MessageBoxButtons.YesNo);

            if (result == DialogResult.Yes)
            {
                Console.ForegroundColor = ConsoleColor.Magenta;
                Logger.Log($" [extract : hash {utils.Hash.Evaluate(entry)}]");
                Console.ResetColor();
                entry.ExtractToFile(extractedFilePath, true);
            }
        }

        private void UnpackNonExistingNode(ZipArchiveEntry entry, bool forceOverwrite = false)
        {
            string extractedFilePath = Path.Combine(Directory.GetCurrentDirectory(), "steamui", "skins", entry.FullName);

            // Check if the entry is a file (not a directory)
            if (!string.IsNullOrEmpty(entry.Name))
            {
                // Ensure the directory structure exists before extracting the file
                string directoryPath = Path.GetDirectoryName(extractedFilePath);

                if (string.IsNullOrEmpty(directoryPath))
                {
                    Logger.Log($"directoryPath was unexpectedly null.");
                    return;
                }

                if (!Directory.Exists(directoryPath))
                {
                    Logger.Log($"Creating directory: {directoryPath}");
                    Directory.CreateDirectory(directoryPath);
                }

                Logger.Log($"[+] {entry.FullName}", true);

                var cloudHash = utils.Hash.Evaluate(entry).ToLower();

                if (!File.Exists(extractedFilePath))
                {
                    Logger.Log($" [create]", ConsoleColor.Magenta, true);
                    entry.ExtractToFile(extractedFilePath, true);
                }

                // Check if the file already exists in the extracted folder
                if (forceOverwrite)
                {
                    Logger.Log($" [overwrite : hash {utils.Hash.Evaluate(entry)}]", ConsoleColor.Magenta);
                    entry.ExtractToFile(extractedFilePath, true);
                }
                else
                {
                    Logger.Log(" [good]", ConsoleColor.Green, true);

                    if (UpdateForce.forceUpToDate)
                    {
                        Logger.Log($" [--force : hash {utils.Hash.Evaluate(entry)}]", ConsoleColor.Magenta);
                        entry.ExtractToFile(extractedFilePath, true);
                    }
                    else
                    {
                        var diskHash = utils.Hash.EvaluatePath(extractedFilePath).ToLower();

                        if (diskHash != cloudHash)
                        {
                            Logger.Log(" [changed]", ConsoleColor.DarkYellow);
                            string message = $"A portion of ['{ThemeName}'] is not synced with the latest release. This could be due to a missed theme update or manual file editing.\n\nAffected file:\n{extractedFilePath}\n\nDo you want to overwrite this file with the latest version?";

                            DialogResult result = MessageBox.Show(message, "Millennium - Potential Conflict", MessageBoxButtons.YesNoCancel);

                            if (result == DialogResult.Yes)
                            {
                                Logger.Log($" [mismatch : hash {utils.Hash.Evaluate(entry)}]", ConsoleColor.Magenta);
                                entry.ExtractToFile(extractedFilePath, true);
                            }
                        }
                        else Logger.Log(string.Empty);
                    }
                }
            }
            else
            {
                Logger.Log($"[+] {extractedFilePath}", true);

                // Entry is a directory, ensure the directory structure exists
                if (!Directory.Exists(extractedFilePath))
                {
                    Logger.Log($" [bad | creating]", ConsoleColor.Red);
                    Directory.CreateDirectory(extractedFilePath);
                }
                else
                {
                    Logger.Log($" [good]", ConsoleColor.Green);
                }
            }
        }

        private void ParseTheme(ZipArchive archive, string nativeName)
        {
            bool found = false;

            foreach (ZipArchiveEntry entry in archive.Entries)
            {
                if (entry.Name != "skin.json")
                {
                    continue;
                }

                found = true;
                using (StreamReader reader = new StreamReader(entry.Open()))
                {
                    string content = reader.ReadToEnd();

                    try
                    {
                        dynamic json = JsonConvert.DeserializeObject(content);

                        if (json?.name != null)
                        {
                            ThemeName = json.name;
                        }
                        else
                        {
                            ThemeName = nativeName;
                        }
                    }
                    catch (Exception ex)
                    {
                        Logger.Log(ex.ToString());
                        ThemeName = nativeName;
                    }

                }
            }

            if (!found)
            {
                ThemeName = nativeName;
            }
        }

        private bool InspectArchive()
        {
            List<FileInformation> nodes = GetUpdateNodes(ApiUrl);

            //if (nodes.Count == 0)
            //{
            //    return;
            //}

            try
            {
                using (ZipArchive archive = ZipFile.OpenRead(ZipArchiveName))
                {
                    ParseTheme(archive, archive.Entries[0].FullName);

                    foreach (ZipArchiveEntry entry in archive.Entries)
                    {
                        bool SkipNode = false;

                        foreach (var updateNode in nodes)
                        {
                            string newCommit = Path.Combine(archive.Entries[0].FullName, updateNode.FileName);

                            if (entry.FullName != newCommit)
                            {
                                continue;
                            }

                            Logger.Log($"[+] [file: {updateNode.FileName}] was updated. Verifying... ", true);

                            string evaluatedHash = utils.Hash.Evaluate(entry).ToLower();
                            string cloudHash = updateNode.ShaHash.ToLower();

                            if (evaluatedHash != cloudHash)
                            {
                                Logger.Log($"[bad]", ConsoleColor.Red);
                                continue;
                            }
                            Logger.Log($"[okay]", ConsoleColor.Green);

                            SkipNode = true;
                            this.UnpackNonExistingNode(entry, true);
                        }

                        if (!SkipNode)
                        {
                            this.UnpackNonExistingNode(entry);
                        }
                    }
                }
                return true;
            }
            catch (IOException e)
            {
                Logger.Log($"An error occurred: {e.Message}");
                return false;
            }
        }

        private List<FileInformation> GetUpdateNodes(string url)
        {
            Logger.Log("[+] Getting Update Nodes: " + url);

            string httpResponse = Http.Get(url).Result;

            List<FileInformation> updatedNodes = new List<FileInformation>();

            if (string.IsNullOrEmpty(httpResponse))
            {
                return updatedNodes;
            }

            dynamic json = JsonConvert.DeserializeObject(httpResponse);

            if (json == null)
            {
                return updatedNodes;
            }

            foreach (dynamic file in json.files)
            {

                string filename = file?.filename;
                string sha = file?.sha;

                if (filename == null || sha == null)
                {
                    continue;
                }

                updatedNodes.Add(new FileInformation(filename, sha));
            }

            return updatedNodes;
        }
    }
}
