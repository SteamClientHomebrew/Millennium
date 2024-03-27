using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using updater.utils;
public class HashObject
{
    public string hash { get; set; }
    public string commit { get; set; }
}

internal class HashManager
{
    private static readonly string filePath = ".millennium/manager/hashes-backend.json";

    private static bool isValid(string s)
    {
        try
        {
            JToken.Parse(s);
            return true;
        }
        catch (JsonReaderException)
        {
            return false;
        }
    }
    
    public static bool GetHashFromTheme(string owner, string repo, string currentHash, out string hash)
    {
        string hashedName = Hash.Evaluate(owner + repo);

        try
        {
            List<HashObject> hashObjects;

            if (File.Exists(filePath))
            {
                string jsonContent = File.ReadAllText(filePath);

                if (!string.IsNullOrEmpty(jsonContent) && isValid(jsonContent))
                {
                    // Deserialize the JSON content into a list of HashObjects
                    hashObjects = JsonConvert.DeserializeObject<List<HashObject>>(jsonContent);

                    // Check if the hash name exists in the list
                    var existingHashObject = hashObjects?.Find(obj => obj.hash == hashedName);
                    if (existingHashObject != null)
                    {
                        // The hash name exists, return its hash data
                        hash = existingHashObject.commit;
                        return true;
                    }
                }
                else
                {
                    // If the file exists but has no valid JSON data, initialize the list
                    hashObjects = new List<HashObject>();
                }
            }
            else
            {
                // If the file doesn't exist, initialize the list
                hashObjects = new List<HashObject>();
            }

            // Add a new HashObject to the list
            hashObjects.Add(new HashObject { hash = hashedName, commit = currentHash });

            // Serialize the list back to JSON
            string updatedJsonContent = JsonConvert.SerializeObject(hashObjects, Formatting.Indented);

            try
            {
                // Create directories recursively
                Directory.CreateDirectory(Path.GetDirectoryName(filePath));
            }
            catch (Exception e)
            {
                Console.WriteLine($"Error: {e.Message}");
            }

            // Write to the file (create if it doesn't exist)
            File.WriteAllText(filePath, updatedJsonContent);

            // Set the out variable
            hash = currentHash;
            return false;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error reading or deserializing/serializing JSON: {ex.Message}");
            hash = null;
            return false;
        }
    }

    public static void SetHash(string owner, string repo, string hashData)
    {
        string hashedName = Hash.Evaluate(owner + repo);

        Logger.Log($"[+] Attempting to set new hash for: {hashedName}");

        try
        {
            List<HashObject> hashObjects;

            if (File.Exists(filePath))
            {
                string jsonContent = File.ReadAllText(filePath);
                hashObjects = JsonConvert.DeserializeObject<List<HashObject>>(jsonContent);
            }
            else
            {
                hashObjects = new List<HashObject>();
            }

            var existingHashObject = hashObjects?.Find(obj => obj.hash == hashedName);

            if (existingHashObject != null)
            {
                existingHashObject.commit = hashData;
            }
            else
            {
                hashObjects.Add(new HashObject { hash = hashedName, commit = hashData });
            }

            string updatedJsonContent = JsonConvert.SerializeObject(hashObjects, Formatting.Indented);
            File.WriteAllText(filePath, updatedJsonContent);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error reading or deserializing/serializing JSON: {ex.Message}");
        }
    }
}