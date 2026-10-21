.pragma library
// The time to read a number of words, at 238 words a minute: the average
// speed of silent reading for adults.

var wordsPerMinute = 238;

// "under 1 min", "12 min", or "2 h 5 min". An empty text for no words.
function label(words) {
    if (words <= 0)
        return "";
    const minutes = Math.round(words / wordsPerMinute);
    if (minutes < 1)
        return "under 1 min";
    if (minutes < 60)
        return minutes + " min";
    const rest = minutes % 60;
    return Math.floor(minutes / 60) + " h" + (rest > 0 ? " " + rest + " min" : "");
}
