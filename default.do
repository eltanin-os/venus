#!/usr/bin/automate -s std-default,std-c
@program{
	transform{
		${%%glob-edit}
		glob:src/*.c
		edit:sed "s;.c$$;;"
	}
}
$@program{
	library{
		tertium
	}
}
@manpage{
	man/*
}
install-store{
	command:cp -R pm ${DESTDIR}${PREFIX}/venus-store
}
