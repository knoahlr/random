# invoke SourceDir generated makefile for random.pem4f
random.pem4f: .libraries,random.pem4f
.libraries,random.pem4f: package/cfg/random_pem4f.xdl
	$(MAKE) -f C:\Users\NOAHWO~1\Desktop\P_PR_1\repo\Projects\ARM_Test_1\random/src/makefile.libs

clean::
	$(MAKE) -f C:\Users\NOAHWO~1\Desktop\P_PR_1\repo\Projects\ARM_Test_1\random/src/makefile.libs clean

