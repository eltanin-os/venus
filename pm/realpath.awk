BEGIN {
	if (target == ".") target = "/"
	target = "/" substr(target, 1 + index(target, "/"))
	if (current == ".") current = "/"
	current = "/" substr(current, 1 + index(current, "/"))
	appendix = substr(target, 2)
	relative = ""
	while (1) {
		appendix = target
		sub("^" current "/", "", appendix)
		if (!(current != "/" && appendix == target)) break
		if (current == appendix) {
			relative = (relative == "" ? "." : relative)
			sub(/^\//, "", relative)
			print relative
			exit 0
		}
		sub(/\/[^\/]*\/?$/, "", current)
		relative = relative (relative == "" ? "" : "/") ".."
	}
	sub(/^\//, "", appendix)
	relative = relative (relative == "" ? "" : (appendix == "" ? "" : "/")) appendix
	print relative
}
