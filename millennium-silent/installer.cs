using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace millennium_silent
{
    internal class installer
    {
        private static readonly string steam_path = steam.fetch_path();
        public static void uninstall_millennium()
        {
            bool uninstall_fail = false;

            foreach (var file in github.get_repo().Result["assets"].ToObject<JArray>())
            {
                string file_name = Path.Combine(steam_path, file.ToObject<JObject>().Value<string>("name"));

                if (!File.Exists(file_name))
                {
                    Console.WriteLine($" [-] skipping {file_name}, doesn't exist");
                    continue;
                }

                Console.WriteLine($" [+] removing {file_name}");
                try
                {
                    File.Delete(file_name);
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"failed to uninstall {file_name}, exception: {ex.Message}");
                    uninstall_fail = true;
                }
            }
            Console.WriteLine(uninstall_fail ? "uninstaller failed..." : "uninstalled millennium successfully...");
        }

        public static async Task install_millennium()
        {
            var releaseInfo = new github.release_info();
            var jsonObject = github.get_repo().Result;

            releaseInfo.body = jsonObject.Value<string>("body");
            releaseInfo.version = jsonObject.Value<string>("tag_name");
            releaseInfo.attachments = new List<github.attachment>();

            foreach (var attachmentToken in jsonObject["assets"].ToObject<JArray>())
            {
                var attachment = attachmentToken.ToObject<JObject>();
                var attachmentInfo = new github.attachment
                {
                    name = attachment.Value<string>("name"),
                    download_count = attachment.Value<int>("download_count"),
                    file_size = Math.Round(attachment.Value<long>("size") / 1048576.0, 2),
                    download_url = attachment.Value<string>("browser_download_url")
                };
                releaseInfo.attachments.Add(attachmentInfo);
            }

            Console.WriteLine($"Millennium v{releaseInfo.version}");
            Console.WriteLine("Release notes: " + releaseInfo.body);
            Console.WriteLine("queued downloads:");
            Console.WriteLine();
            foreach (var attachment in releaseInfo.attachments)
            {
                Console.WriteLine("Name: " + attachment.name);
                Console.WriteLine("Download Count: " + attachment.download_count);
                Console.WriteLine("File Size: " + attachment.file_size + " MB");
                Console.WriteLine("downloading from: " + attachment.download_url);
                await github.download(attachment.download_url, Path.Combine(steam_path, attachment.name));
                Console.WriteLine("downloaded to: " + Path.Combine(steam_path, attachment.name));
                Console.WriteLine();
            }

            Console.WriteLine($"millennium is now integrated with your steam. have fun \n[active-skin: {user.fetch_desired_skin()}]");
        }
    }
}
