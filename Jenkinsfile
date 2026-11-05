// The public repository needs no Jenkins credentials.
def checkoutLibivy(String revision, boolean trackChanges = false) {
    return checkout(changelog: trackChanges, poll: trackChanges, scm: [
        $class: 'GitSCM',
        branches: [[name: revision]],
        userRemoteConfigs: [[url: 'https://github.com/lii-enac/libivy.git']]
    ])
}

def selectedBuild(String name) {
    return !params.BUILD_ONLY?.trim() || params.BUILD_ONLY == 'all' || params.BUILD_ONLY == name
}

def notifyResult(String status, String scope = 'Pipeline') {
    // Same default recipients as djnn-cpp; job variables can override them.
    def recipients = [env.EMAIL1, env.EMAIL2].findAll { it?.trim() }.join(', ')
    if (!recipients) {
        recipients = 'mathieu.poirier@enac.fr, stephane.conversy@enac.fr'
    }
    def icon = status == 'RECOVERED' ? '✅' : '❌'
    echo "Sending libivy ${scope} ${status} notification to ${recipients}"
    emailext(
        to: recipients,
        subject: "${icon} libivy ${scope} ${status} - ${env.JOB_NAME} #${env.BUILD_NUMBER}",
        mimeType: 'text/plain',
        attachLog: status in ['FAILURE', 'UNSTABLE', 'ABORTED'],
        compressLog: true,
        body: """Stage: ${scope}
Result: ${status}
Build: ${env.BUILD_URL}
Commit: ${env.LIBIVY_REVISION ?: 'checkout failed'}

Console: ${env.BUILD_URL}console

scan-build and clang-tidy reports (available after artifact archiving):
${env.BUILD_URL}artifact/
"""
    )
}

pipeline {
    agent none

    parameters {
        choice(
            name: 'BUILD_ONLY',
            choices: ['', 'all', 'macos', 'ubuntu', 'windows', 'windows-wsl'],
            description: 'Empty/all builds every platform. macOS compilation and both macOS analyses always run; this selection limits the other platform builds.'
        )
    }

    options {
        buildDiscarder(logRotator(numToKeepStr: '10', artifactNumToKeepStr: '10'))
        disableConcurrentBuilds()
        skipDefaultCheckout(true)
        timestamps()
    }

    triggers {
        pollSCM('H/5 * * * *')
    }

    stages {
        stage('Build macOS') {
            agent { label 'macos12' }
            options { timeout(time: 15, unit: 'MINUTES') }
            environment {
                PATH = '/opt/homebrew/bin:/opt/homebrew/sbin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin'
            }
            steps {
                dir('libivy') {
                    deleteDir()
                    script {
                        def revision = checkoutLibivy('*/master', true)
                        env.LIBIVY_REVISION = revision.GIT_COMMIT
                        currentBuild.description = "libivy ${env.LIBIVY_REVISION.take(12)}"
                    }
                    // Retain the failure but still generate analysis reports for this commit.
                    catchError(buildResult: 'FAILURE', stageResult: 'FAILURE') {
                        sh '''
                            set -eu
                            ALL_PROXY="$PROXY_URL" make install-pkgdeps
                            make -j8 all tools
                            make check
                        '''
                    }
                }
            }
            post {
                failure { script { notifyResult('FAILURE', 'Build macOS') } }
            }
        }

        stage('Analyses macOS and other platforms') {
            // No failFast: one platform/analysis failure must not cancel the others.
            parallel {
                stage('Static analysis macOS') {
                    agent { label 'macos12' }
                    options { timeout(time: 45, unit: 'MINUTES') }
                    environment {
                        // LLVM is selected explicitly; Apple compilers remain the build toolchain.
                        PATH = '/opt/homebrew/bin:/opt/homebrew/sbin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin'
                    }
                    stages {
                        stage('Checkout for analysis') {
                            steps {
                                dir('libivy-analysis') {
                                    deleteDir()
                                    script { checkoutLibivy(env.LIBIVY_REVISION) }
                                }
                            }
                            post {
                                failure { script { notifyResult('FAILURE', 'Checkout for analysis') } }
                            }
                        }
                        stage('scan-build') {
                            steps {
                                dir('libivy-analysis') {
                                    catchError(buildResult: 'FAILURE', stageResult: 'FAILURE') {
                                        sh 'python3 distrib/script/analyze_macos.py scan-build --report-dir reports/scan-build --jobs 8'
                                    }
                                }
                            }
                            post {
                                failure { script { notifyResult('FAILURE', 'scan-build') } }
                            }
                        }
                        stage('clang-tidy') {
                            steps {
                                dir('libivy-analysis') {
                                    catchError(buildResult: 'FAILURE', stageResult: 'FAILURE') {
                                        sh 'python3 distrib/script/analyze_macos.py tidy --report-dir reports/tidy --jobs 4'
                                    }
                                }
                            }
                            post {
                                failure { script { notifyResult('FAILURE', 'clang-tidy') } }
                            }
                        }
                    }
                    post {
                        always {
                            dir('libivy-analysis') {
                                sh '''
                                    if [ -d reports ]; then
                                        /usr/bin/zip -qr analysis-reports.zip reports
                                    fi
                                '''
                                archiveArtifacts artifacts: 'reports/**,analysis-reports.zip', allowEmptyArchive: true
                            }
                        }
                    }
                }

                stage('Build Ubuntu') {
                    when {
                        beforeAgent true
                        expression { selectedBuild('ubuntu') }
                    }
                    agent { label 'ubuntu' }
                    options { timeout(time: 15, unit: 'MINUTES') }
                    steps {
                        dir('libivy') {
                            deleteDir()
                            script { checkoutLibivy(env.LIBIVY_REVISION) }
                            sh '''
                                set -eu
                                make version
                                ALL_PROXY="$PROXY_URL" sudo make install-pkgdeps
                                make -j8 static-libs shared-libs tools pkgconf
                                make check
                            '''
                        }
                    }
                    post {
                        failure { script { notifyResult('FAILURE', 'Build Ubuntu') } }
                    }
                }

                stage('Build Windows MSYS2') {
                    when {
                        beforeAgent true
                        expression { selectedBuild('windows') }
                    }
                    agent { label 'win10' }
                    tools { git 'Git-MSYS' }
                    options { timeout(time: 15, unit: 'MINUTES') }
                    steps {
                        dir('libivy') {
                            deleteDir()
                            script { checkoutLibivy(env.LIBIVY_REVISION) }
                            bat 'set "ALL_PROXY=%PROXY_URL%" && make install-pkgdeps'
                            bat '''
                                @echo on
                                make version
                                if errorlevel 1 exit /b 1
                                make -j8 static-libs shared-libs tools pkgconf
                                if errorlevel 1 exit /b 1
                                make check
                                if errorlevel 1 exit /b 1
                            '''
                        }
                    }
                    post {
                        failure { script { notifyResult('FAILURE', 'Build Windows MSYS2') } }
                    }
                }

                stage('Build Windows WSL') {
                    when {
                        beforeAgent true
                        expression { selectedBuild('windows-wsl') }
                    }
                    agent { label 'wsl' }
                    options { timeout(time: 15, unit: 'MINUTES') }
                    steps {
                        dir('libivy') {
                            deleteDir()
                            script { checkoutLibivy(env.LIBIVY_REVISION) }
                            sh '''
                                set -eu
                                make version
                                make -j8 static-libs shared-libs tools pkgconf
                                make check
                            '''
                        }
                    }
                    post {
                        failure { script { notifyResult('FAILURE', 'Build Windows WSL') } }
                    }
                }
            }
        }
    }

    post {
        success {
            script {
                if (currentBuild.previousBuild?.result in ['FAILURE', 'UNSTABLE']) {
                    notifyResult('RECOVERED')
                }
            }
        }
        failure { script { notifyResult('FAILURE') } }
        unstable { script { notifyResult('UNSTABLE') } }
        aborted { script { notifyResult('ABORTED') } }
    }
}
