function sendDelete(path) {
    fetch(path, { method: "DELETE" })
        .then(res => res.text().then(body => {
            document.getElementById("deleteResult").innerText =
                "DELETE " + path + " -> " + res.status + " " + res.statusText;
        }))
        .catch(err => alert("Error: " + err));
}

document.getElementById("uploadForm").onsubmit = function (e) {
    e.preventDefault();
    const formData = new FormData(this);

    fetch("/upload", {
        method: "POST",
        body: formData
    })
    .then(res => res.text())
    .then(text => {
        document.getElementById("uploadResult").innerText = text;
    });
};
