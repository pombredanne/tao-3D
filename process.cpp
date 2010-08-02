// ****************************************************************************
//  process.cpp                                                     Tao project
// ****************************************************************************
// 
//   File Description:
// 
//     Encapsulate a process with a streambuf we can easily write to
// 
// 
// 
// 
// 
// 
// 
// 
// ****************************************************************************
// This document is released under the GNU General Public License.
// See http://www.gnu.org/copyleft/gpl.html and Matthew 25:22 for details
//  (C) 1992-2010 Christophe de Dinechin <christophe@taodyne.com>
//  (C) 2010 Jerome Forissier <jerome@taodyne.com>
//  (C) 2010 Taodyne SAS
// ****************************************************************************

#include "process.h"
#include "tao_utf8.h"
#include "errors.h"
#include "options.h"

#include <cassert>
#include <QMessageBox>
#include <QApplication>
#include <QRegExp>
#include <QDir>


TAO_BEGIN

ulong Process::snum = 0;

Process::Process(size_t bufSize)
// ----------------------------------------------------------------------------
//   Create a QProcess without starting it yet
// ----------------------------------------------------------------------------
    : commandLine(""), id(NULL),
      aborted(false), errPos(0), percent(0)
{
    initialize(bufSize);
}


Process::Process(const QString &cmd,
                 const QStringList &args,
                 const QString &wd,
                 bool  startImmediately,
                 size_t bufSize)
// ----------------------------------------------------------------------------
//   Create a QProcess
// ----------------------------------------------------------------------------
    : commandLine(""), cmd(cmd), args(args), wd(wd),
      id(NULL), aborted(false),
      errPos(0), percent(0)
{
    setWorkingDirectory(wd);
    initialize(bufSize);
    if (startImmediately)
        start();
}


Process::~Process()
// ----------------------------------------------------------------------------
//    Make sure we are done with all the data for that process
// ----------------------------------------------------------------------------
{
    if (pbase())
        done();
}


void Process::start(const QString &cmd, const QStringList &args,
                    const QString &wd)
// ----------------------------------------------------------------------------
//   Start child process
// ----------------------------------------------------------------------------
{
    commandLine = cmd + " " + args.join(" ");
    if (!wd.isEmpty())
    {
        // Note: it is possible that directory wd does not exist. It must not
        // be rejected here or clone won't work anymore.
        setWorkingDirectory(wd);
    }

    IFTRACE(process)
        std::cerr << "Process " << num << ": " << +commandLine
                  << " (wd " << +workingDirectory() << ")\n";

    QProcess::start(cmd, args);
}


void Process::start()
// ----------------------------------------------------------------------------
//   Start child process
// ----------------------------------------------------------------------------
{
    start(cmd, args, wd);
}


bool Process::done(text *errors, text *output)
// ----------------------------------------------------------------------------
//    Return true if the process was successful
// ----------------------------------------------------------------------------
{
    bool ok = true;

    // Flush streambuf
    if (pbase())
        if (sync())
            ok = false;
    delete[] pbase();
    setp(NULL, NULL);

    // Close QProcess
    closeWriteChannel();
    if (state() != NotRunning)
        if (!waitForFinished())
            ok = false;

    int rc = exitCode();
    if (rc)
        ok = false;

    if (output)
        *output = +out;
    if (errors)
        *errors = +err;

    return ok;
}


bool Process::failed()
// ----------------------------------------------------------------------------
//   Return true if the process crashed or returned a non-zero value
// ----------------------------------------------------------------------------
{
  return (exitStatus() != QProcess::NormalExit) || exitCode();
}


void Process::initialize(size_t bufSize)
// ----------------------------------------------------------------------------
//   Initialize the process and streambuf
// ----------------------------------------------------------------------------
{
    num = snum++;

    // Allocate data to be used by the streambuf
    char *ptr = new char[bufSize];
    setp(ptr, ptr + bufSize);

    // Set up the stderr/stdout signal mechanism
    connect(this, SIGNAL(readyReadStandardOutput()),
            this, SLOT(readStandardOutput()));
    connect(this, SIGNAL(readyReadStandardError()),
            this, SLOT(readStandardError()));

    // Set up debug traces
    IFTRACE(process)
    {
        connect(this, SIGNAL(finished(int,QProcess::ExitStatus)),
                this, SLOT(debugProcessFinished(int,QProcess::ExitStatus)));
        connect(this, SIGNAL(error(QProcess::ProcessError)),
                this, SLOT(debugProcessError(QProcess::ProcessError)));
    }
}


void Process::setEnvironment()
// ----------------------------------------------------------------------------
//   Set environment variables that have an influence on child process
// ----------------------------------------------------------------------------
{
}


int Process::overflow(int c)
// ------------------------------------------------------------------------
//   Override std::streambuf::overflow() to pipe to process
// ------------------------------------------------------------------------
{
    int error = sync();
    if (!error && c != traits_type::eof())
    {
        assert(pbase() != epptr());
        error = sputc(c);
    }
    return error;
}


int Process::sync()
// ------------------------------------------------------------------------
//   Overriding std::streambuf::sync(), write data to process
// ------------------------------------------------------------------------
{
    if (pbase() != pptr())
    {
        QProcess::write(pbase(), pptr() - pbase());
        setp(pbase(), epptr());
    }
    return 0;
}


void Process::readStandardOutput()
// ------------------------------------------------------------------------
//   Append new stdout data to buffer
// ------------------------------------------------------------------------
{
    QByteArray newOut = QProcess::readAllStandardOutput();
    out.append(newOut);
    emit standardOutputUpdated(newOut);
}


void Process::readStandardError()
// ------------------------------------------------------------------------
//   Append new stderr data to buffer
// ------------------------------------------------------------------------
{
    QByteArray newErr = QProcess::readAllStandardError();
    err.append(newErr);
    emit standardErrorUpdated(newErr);
}


void Process::debugProcessFinished(int exitCode, QProcess::ExitStatus st)
// ------------------------------------------------------------------------
//   Print debug traces to stderr when process finishes
// ------------------------------------------------------------------------
{
    QString type = exitStatusToString(st);
    std::cerr << +tr("Process %1 finished (exit type: %2, exit code: %3)\n")
                     .arg(num).arg(type).arg((int)exitCode);
    if (!out.isEmpty())
        std::cerr << +tr("Process %1 stdout:\n%2Process %3 stdout end\n")
                         .arg(num).arg(out).arg(num);
    if (!err.isEmpty())
        std::cerr << +tr("Process %1 stderr:\n%2Process %3 stderr end\n")
                         .arg(num).arg(err).arg(num);
}


void Process::debugProcessError(QProcess::ProcessError error)
// ------------------------------------------------------------------------
//   Print debug traces to stderr in case of process error
// ------------------------------------------------------------------------
{
    QString type = processErrorToString(error);
    std::cerr << +tr("Process %1 error: %2\n").arg(num)
                                              .arg(type);
    if (!err.isEmpty())
        std::cerr << +tr("  stderr: %1\n").arg(err);
}


QString Process::processErrorToString(QProcess::ProcessError error)
// ------------------------------------------------------------------------
//   Convert error code to text
// ------------------------------------------------------------------------
{
    switch (error)
    {
    case QProcess::FailedToStart:  return tr("Failed to start");
    case QProcess::Crashed:        return tr("Crashed");
    case QProcess::Timedout:       return tr("Timed out");
    case QProcess::WriteError:     return tr("Write error");
    case QProcess::ReadError:      return tr("Read error");
    case QProcess::UnknownError:   return tr("Unknown error");
    default:                       return tr("???");
    }
}


QString Process::exitStatusToString(QProcess::ExitStatus status)
// ------------------------------------------------------------------------
//   Convert error code to text
// ------------------------------------------------------------------------
{
    switch (status)
    {
    case QProcess::NormalExit:  return tr("Normal exit");
    case QProcess::CrashExit:   return tr("Crash exit");
    default:                    return tr("???");
    }
}

TAO_END
