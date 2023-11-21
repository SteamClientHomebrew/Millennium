using System;
using System.Net.Http;
using Newtonsoft.Json;
using System.Threading.Tasks;
using System.Collections.Generic;
using System.Net;
using System.Security.Policy;
using System.IO;

public class FileItem
{
    public string Name { get; set; }
    public string Dl_Url { get; set; }
    public int Dl_Count { get; set; }
    public string FileSize { get; set; }
    public void DownloadFileSync(string parentPathExcludingName)
    {
        Console.WriteLine($"Downloading file {Dl_Url}");

        HttpWebRequest request = (HttpWebRequest)WebRequest.Create(Dl_Url);
        request.UserAgent = "Millennium.Installer";

        try
        {
            using (HttpWebResponse response = (HttpWebResponse)request.GetResponse())
            using (Stream responseStream = response.GetResponseStream())
            using (FileStream fileStream = File.Create(parentPathExcludingName + "/" + Name))
            {
                responseStream.CopyTo(fileStream);
            }

            Console.WriteLine($"File downloaded to: {parentPathExcludingName + "/" + Name}");
        }
        catch (WebException e)
        {
            Console.WriteLine($"HTTP request failed with the following error: {e.Message}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"An error occurred: {ex.Message}");
        }
    }
}

public class GitHubApiClient
{
    private readonly string _githubUrl;

    public GitHubApiClient(string githubUrl)
    {
        _githubUrl = githubUrl;
    }

    public async Task<IEnumerable<FileItem>> GetDownloadListAsync()
    {
        List<FileItem> items = new List<FileItem>();
        dynamic json = await GetJsonAsync(_githubUrl);

        foreach (var asset in json.assets)
        {
            var fileItem = new FileItem
            {
                Name = asset.name,
                Dl_Url = asset.browser_download_url,
                Dl_Count = asset.download_count,
                FileSize = asset.size,
            };
            items.Add(fileItem);
        }

        return items;
    }

    private async Task<dynamic> GetJsonAsync(string url)
    {
        using (var client = new HttpClient())
        {
            client.DefaultRequestHeaders.UserAgent.ParseAdd("Millennium.Installer");

            var response = await client.GetStringAsync(url);
            return JsonConvert.DeserializeObject<dynamic>(response);
        }
    }
}
