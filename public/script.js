function showMessage()
{
    document.getElementById("message").innerHTML =
        "🎉 JavaScript is working! Your C++ HTTP server successfully served script.js.";
}

console.log("JavaScript loaded successfully.");

window.onload = function ()
{
    console.log("Page loaded through your C++ Web Server.");
};