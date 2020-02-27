# Homework4--Simple Web Server

## Usage
`$ ./webserver port "/path/to/your/webserver/docroot"`

## Features 
### GET a static object
1. If a requested object can be found, your server has to return "200 OK" status as well as the proper headers and content for the browser to render the document correctly.

2. If a requested object does not exist, your server has to return a "403 FORBIDDEN" status and provide error messages.

3. If a requested object is inaccessible, your server has to return a "404 NOT FOUND" status and provide error messages.

4. If a requested object is an accessible directory, please follow the descriptions in the next section.

5. If a requested URL contains a question mark (?), you can simply ignore the question mark and all texts after the mark.

6. **Note that we INTENTIONALLY use 403 error code for missing objects and 404 error codes for inaccessible objects.**

### GET a directory

1. If a requested object is a directory, your program have to check whether the requested directory has a slash (/) at the end of the URL.

2. If the URL does not have a slash suffix, you have to respond a "301 MOVE PERMANENTLY" status and a new location with a slash appended to the original requested URL.

3. If a slash is already given in the URL, your server has to search for a default index file, e.g., index.html, in the directory.

4. If there is a readable index.html file, you can simply send the content of index.html back to the browser.

5. If there is a index.html file, but it is not readable, you can simply send "403 FORBIDDEN" status to the browser.

6. If there is not a readable index.html file, but the directory is readable, you have to list the files and directories found in the requested directory. The list can be in either a plain-text document or a html document.

7. It would be a plus if your response is a html document **with hyperlinks** point to the location of files and directories in the requested directory.

8. If there is not a readable index.html file, and the directory is not readable, you have to send "404 NOT FOUND" status to the browser.

### Execute CGI programs
1. Implement CGI execution using GET requests: Environment variable - REQUEST_METHOD=GET

2. If a question mark (?) is used in the URL, add the content after (?) into a environment variable QUERY_STRING.

3. Execute the sample CGI provided by this homework. You will have to setup a pipe to forward outputs from a CGI script to the browser.

