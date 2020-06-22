/*
 * SessionPanmirrorBibliography.cpp
 *
 * Copyright (C) 2009-16 by RStudio, Inc.
 *
 * Unless you have received this program directly from RStudio pursuant
 * to the terms of a commercial license agreement with RStudio, then
 * this program is licensed to you under the terms of version 3 of the
 * GNU Affero General Public License. This program is distributed WITHOUT
 * ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THOSE OF NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Please refer to the
 * AGPL (http://www.gnu.org/licenses/agpl-3.0.txt) for more details.
 *
 */

#include "SessionPanmirrorBibliogrpahy.hpp"

#include <boost/bind.hpp>

#include <shared_core/Error.hpp>
#include <core/Hash.hpp>
#include <core/Exec.hpp>
#include <core/FileSerializer.hpp>
#include <core/json/JsonRpc.hpp>
#include <core/StringUtils.hpp>

#include <core/system/Process.hpp>

#include <session/projects/SessionProjects.hpp>
#include <session/SessionModuleContext.hpp>

using namespace rstudio::core;

namespace rstudio {
namespace session {
namespace modules {
namespace panmirror {
namespace bibliography {

namespace {

std::vector<FileInfo> projectBibliographies()
{
   std::vector<FileInfo> biblioFiles;
   std::vector<FilePath> projectBibs = module_context::bookdownBibliographies();
   std::transform(projectBibs.begin(),
                  projectBibs.end(),
                  std::back_inserter(biblioFiles),
                  toFileInfo);
   return biblioFiles;
}

const char * const kBiblioJson = "biblio.json";
const char * const kBiblioFiles = "biblio-files";
const char * const kBiblioRefBlock = "biblio-refblock";

// cache the last bibliography we returned (along w/ enough info to construct an etag for the cache)
class BiblioCache
{
public:
   static std::string etag(const std::vector<core::FileInfo>& biblioFiles,
                           const std::string& refBlock)
   {
      std::ostringstream ostr;
      for (auto file : biblioFiles)
          ostr << file.absolutePath() << ":" << file.lastWriteTime();
      if (refBlock.length() > 0)
         ostr << hash::crc32HexHash(refBlock);
      return ostr.str();
   }

public:
   void update(const json::Object& biblioJson,
               const std::vector<core::FileInfo>& biblioFiles,
               const std::string& refBlock)
   {
      // update members
      biblioJson_ = biblioJson;
      biblioFiles_ = biblioFiles;
      refBlock_ = refBlock;

      // attempt to write to storage
      Error error = writeToStorage();
      if (error)
         LOG_ERROR(error);
   }

   void clear()
   {
      biblioJson_.clear();
      biblioFiles_.clear();
      refBlock_.clear();
   }

   std::string etag()
   {
      return etag(biblioFiles_, refBlock_);
   } 

   bool isFor(const std::vector<core::FileInfo>& biblioFiles,
              const std::string& refBlock)
   {
      if (refBlock == refBlock_ && biblioFiles.size() == biblioFiles_.size())
      {
         return std::equal(biblioFiles.begin(), biblioFiles.end(), biblioFiles_.begin(),
           [](const FileInfo& a, const FileInfo& b)
            {
               return a.absolutePath() == b.absolutePath();
            }
         );
      }
      else
      {
         return false;
      }
   }

   void setResponse(json::JsonRpcResponse* pResponse)
   {
      json::Object result;
      result["etag"] = etag();
      result["bibliography"] = biblioJson_;
      pResponse->setResult(result);
   }

   Error readFromStorage()
   {
      clear();

      FilePath storageDir = storagePath();
      FilePath biblioJsonPath = storageDir.completeChildPath(kBiblioJson);
      FilePath biblioFilesPath = storageDir.completeChildPath(kBiblioFiles);
      FilePath refBlockPath = storageDir.completeChildPath(kBiblioRefBlock);
      if (biblioJsonPath.exists() && biblioFilesPath.exists() && refBlockPath.exists())
      {
         std::string biblio;
         std::vector<std::string> files;
         ExecBlock readBlock;
         readBlock.addFunctions()
            (boost::bind(readString, biblioJsonPath, &biblio))
            (boost::bind(parseBiblio, &biblio, &biblioJson_))
            (boost::bind(readStringVectorFromFile, biblioFilesPath, &files, true))
            (boost::bind(readBiblioFiles, &files, &biblioFiles_))
            (boost::bind(readString, refBlockPath, &refBlock_))
         ;
         Error error = readBlock.execute();
         if (error)
         {
            clear();
            return error;
         }
         else
         {
            return Success();
         }
      }
      else
      {
         return Success();
      }
   }

   Error writeToStorage()
   {
      FilePath storageDir = storagePath();
      FilePath biblioJsonPath = storageDir.completeChildPath(kBiblioJson);
      FilePath biblioFilesPath = storageDir.completeChildPath(kBiblioFiles);
      FilePath refBlockPath = storageDir.completeChildPath(kBiblioRefBlock);

      std::vector<std::string> files;
      std::transform(biblioFiles_.begin(), biblioFiles_.end(),
                     std::back_inserter(files),
                     boost::bind(&FileInfo::absolutePath, _1));

      ExecBlock writeBlock;
      writeBlock.addFunctions()
         (boost::bind(&FilePath::removeIfExists, &biblioJsonPath))
         (boost::bind(&FilePath::removeIfExists, &biblioFilesPath))
         (boost::bind(&FilePath::removeIfExists, &refBlockPath))
         (boost::bind(writeString, biblioJsonPath, biblioJson_.writeFormatted()))
         (boost::bind(writeBiblioFiles, biblioFilesPath, files))
         (boost::bind(writeString, refBlockPath, refBlock_))
      ;
      return writeBlock.execute();
   }


private:
   FilePath storagePath()
   {
      FilePath path = module_context::scopedScratchPath().completeChildPath("bibliography-index");
      Error error = path.ensureDirectory();
      if (error)
         LOG_ERROR(error);
      return path;
   }

   static Error writeString(const FilePath& filePath, const std::string& str)
   {
      return writeStringToFile(filePath, str);
   }

   static Error readString(const FilePath& filePath, std::string* pStr)
   {
      return readStringFromFile(filePath, pStr);
   }

   // using a pointer for the *in* argument so we can include this in an exec block
   static Error parseBiblio(const std::string* pBiblio, json::Object* pBiblioJson)
   {
      pBiblioJson->clear();
      if (pBiblio->length() > 0)
         return pBiblioJson->parse(*pBiblio);
      else
         return Success();
   }

   static Error writeBiblioFiles(const FilePath& filePath, const std::vector<std::string>& files)
   {
      std::vector<std::string> records;
      std::transform(files.begin(), files.end(), std::back_inserter(records),
         [](const std::string& file) {
            std::ostringstream ostr;
            ostr << FilePath(file).getLastWriteTime()
                 << ":"
                 << file;
            return ostr.str();
         }
      );
      return writeStringVectorToFile(filePath, records);
   }

    // using a pointer for the *in* argument so we can include this in an exec block
   static Error readBiblioFiles(const std::vector<std::string>* pRecords, std::vector<core::FileInfo>* pFileInfos)
   {
      pFileInfos->clear();
      for (auto record : *pRecords)
      {
         // parse record
         std::istringstream istr(record);
         std::time_t lastWriteTime;
         char colon;
         std::string file;
         istr >> lastWriteTime >> colon >> file;

         // create file info using recorded write time
         FilePath filePath(file);
         FileInfo fileInfo(file, filePath.isDirectory(), filePath.getSize(), lastWriteTime);
         pFileInfos->push_back(fileInfo);
      }

      return Success();
   }


private:
   json::Object biblioJson_;
   std::vector<core::FileInfo> biblioFiles_;
   std::string refBlock_;
};
BiblioCache s_biblioCache;


// global logging helper
void logBiblioStatus(const std::string& str)
{
   str.length(); // silience compiler on unused var
   // std::cerr << str << std::endl;
}

void indexProjectCompleted(const std::vector<core::FileInfo>& biblioFiles,
                           const core::system::ProcessResult& result)
{
   if (result.exitStatus == EXIT_SUCCESS)
   {
      json::Array jsonCitations;
      Error error = jsonCitations.parse(result.stdOut);
      if (!error)
      {
         // create bibliography
         json::Object biblio;
         biblio["sources"] = jsonCitations;

         // cache it
         s_biblioCache.update(biblio, biblioFiles, "");

         // status
         logBiblioStatus("Indexed and updated project bibliography");
      }
      else
      {
         LOG_ERROR(error);
      }
   }
   else
   {
      Error error = systemError(boost::system::errc::state_not_recoverable, ERROR_LOCATION);
      error.addProperty("stderr", result.stdErr);
      LOG_ERROR(error);
   }
}

void getBibliographyCompleted(const std::vector<core::FileInfo>& biblioFiles,
                              const std::string& refBlock,
                              const json::JsonRpcFunctionContinuation& cont,
                              const core::system::ProcessResult& result)
{
   json::JsonRpcResponse response;
   if (result.exitStatus == EXIT_SUCCESS)
   {
      json::Array jsonCitations;
      if (json::parseJsonForResponse(result.stdOut, &jsonCitations, &response))
      {
         // create bibliography
         json::Object biblio;
         biblio["sources"] = jsonCitations;

         // cache last successful bibliograpy
         s_biblioCache.update(biblio, biblioFiles, refBlock);

         // status
         logBiblioStatus("Cached getBibliography response");

         // set response
         s_biblioCache.setResponse(&response);
      }
   }
   else
   {
      json::setProcessErrorResponse(result, ERROR_LOCATION, &response);
   }

   // call continuation
   cont(Success(), &response);
}

void pandocGetBibliography(const json::JsonRpcRequest& request,
                           const json::JsonRpcFunctionContinuation& cont)
{
   // response object
   json::JsonRpcResponse response;

   // extract params
   std::string file, refBlock, etag;
   json::Array bibliographiesJson;
   Error error = json::readParams(request.params, &file, &bibliographiesJson, &refBlock, &etag);
   if (error)
   {
      json::setErrorResponse(error, &response);
      cont(Success(), &response);
      return;
   }

   // determine biblio files
   std::vector<FileInfo> biblioFiles;

   // if there are bibliographies passed form the client then use those in preference to the
   // project bibliographies (b/c they will appear after the project bibliographies)
   if (bibliographiesJson.getSize() > 0)
   {
      std::vector<std::string> biblios;
      bibliographiesJson.toVectorString(biblios);
      for (auto biblio : biblios)
      {
         FilePath biblioPath = module_context::resolveAliasedPath(biblio);
         biblioFiles.push_back(FileInfo(biblioPath));
      }
   }
   // is this file part of the current project? if so then use the project bibliographies as the default
   else if (!file.empty() && projects::projectContext().hasProject())
   {
      FilePath filePath = module_context::resolveAliasedPath(file);
      if (filePath.isWithin(projects::projectContext().buildTargetPath()))
      {
         biblioFiles = projectBibliographies();
      }
   }

   // if the filesystem and the cache agree on the etag then we can serve from cache
   if (s_biblioCache.etag() == BiblioCache::etag(biblioFiles, refBlock))
   {
      // if the client side cache agrees w/ s_biblioCache then return just the etag
      // (indicating that the client already has up to date data)
      if (etag.length() > 0 && etag == s_biblioCache.etag())
      {
         // set result to just the etag
         json::Object result;
         result["etag"] = etag;
         response.setResult(result);

         // status
         logBiblioStatus("Resolved getBibliography from cache (CLIENT)");
      }
      else
      {
         // set result from cache
         s_biblioCache.setResponse(&response);

         // status
         logBiblioStatus("Resolved getBibliography from cache");
      }


      cont(Success(), &response);
      return;
   }

   // build args
   std::vector<std::string> args;

   // always pass the biblio files
   for (auto biblioFile : biblioFiles)
      args.push_back(string_utils::utf8ToSystem(biblioFile.absolutePath()));

   // if a ref block is provided then write it to a temporary file and pass it as well
   if (!refBlock.empty())
   {
      FilePath tempYaml = module_context::tempFile("biblio", "yaml");
      Error error = writeStringToFile(tempYaml, refBlock);
      if (error)
         LOG_ERROR(error);
      args.push_back(string_utils::utf8ToSystem(tempYaml.getAbsolutePath()));
   }

   // convert to json
   args.push_back("--bib2json");

   // run pandoc-citeproc
   error = module_context::runPandocCiteprocAsync(args, boost::bind(getBibliographyCompleted, biblioFiles, refBlock, cont, _1));
   if (error)
   {
      json::setErrorResponse(error, &response);
      cont(Success(), &response);
   }
}


void updateProjectBibliography()
{
   std::vector<FileInfo> biblioFiles = projectBibliographies();
   std::vector<std::string> args;
   for (auto biblioFile : biblioFiles)
      args.push_back(string_utils::utf8ToSystem(biblioFile.absolutePath()));
   args.push_back("--bib2json");
   Error error = module_context::runPandocCiteprocAsync(args, boost::bind(indexProjectCompleted, biblioFiles, _1));
   if (error)
      LOG_ERROR(error);
}

void onCheckForBiblioChange(const std::vector<FileInfo>& biblioFiles,
                            const std::vector<core::system::FileChangeEvent>& changes)
{
   for (const core::system::FileChangeEvent& fileChange : changes)
   {
      bool detectedChange = false;
      for (auto biblioFile : biblioFiles)
      {
         if (biblioFile.absolutePath() == fileChange.fileInfo().absolutePath())
         {
            detectedChange = true;
            if (s_biblioCache.isFor(biblioFiles, ""))
            {
               logBiblioStatus("Updating biblio for file change event");
               updateProjectBibliography();
            }
            break;
         }
      }
      if (detectedChange)
         break;
   }
}

void onDeferredInit(bool)
{
   // read index from storage
   Error error = s_biblioCache.readFromStorage();
   if (error)
      LOG_ERROR(error);

   // if we have a project level bibliography then index it proactively (if we haven't already)
   if (projects::projectContext().hasProject())
   {
      std::vector<FileInfo> biblioFiles = projectBibliographies();
      if (biblioFiles.size())
      {
         // update the project bibliography cache if we need to
         if (s_biblioCache.etag() != BiblioCache::etag(biblioFiles, ""))
            updateProjectBibliography();

         // monitor the filesystem to do further updates
         session::projects::FileMonitorCallbacks cb;
         cb.onFilesChanged = boost::bind(onCheckForBiblioChange, biblioFiles, _1);
         projects::projectContext().subscribeToFileMonitor("Bibliography", cb);
      }
   }
}

} // end anonymous namespace

Error initialize()
{   
   module_context::events().onDeferredInit.connect(onDeferredInit);

   ExecBlock initBlock;
   initBlock.addFunctions()
        (boost::bind(module_context::registerAsyncRpcMethod, "pandoc_get_bibliography", pandocGetBibliography))
   ;
   return initBlock.execute();
}


} // end namespace bibliography
} // end namespace panmirror
} // end namespace modules
} // end namespace session
} // end namespace rstudio